#include <SPI.h>
#include <SD.h>
#include <HardwareSerial.h>

// SIM7600 Serial pins
#define SIM7600_TX 17 // ESP32 TX to SIM7600 RX
#define SIM7600_RX 16 // ESP32 RX to SIM7600 TX
#define SD_CS      5  // SD card CS pin

HardwareSerial modem(2); // Use UART2

// APN and server config
String apn = "vodafone"; // Using vodafone as requested
String host = "cellular-data-streamer.web.app";
String path = "/api/upload";

#define MAX_FILES 50
String fileList[MAX_FILES];
int fileCount = 0;

// Helper to send AT command and wait for expected response
bool sendATCommand(String cmd, String expectedResponse, unsigned long timeout, bool printModemRsp = true) {
  if (cmd.length() > 0) {
    Serial.print(">> ");
    Serial.println(cmd);
    modem.println(cmd);
  }

  unsigned long start = millis();
  String response = "";

  while (millis() - start < timeout) {
    while (modem.available()) {
      char c = modem.read();
      response += c;
       if (printModemRsp) {
          Serial.write(c);
       }
    }
     if (response.indexOf(expectedResponse) != -1) {
      return true;
    }
    if (response.indexOf("ERROR") != -1) {
      // Don't immediately return on error, sometimes it's part of a larger response
      // But we can log it
    }
  }
  if (printModemRsp) {
    Serial.println("\n[ERROR] Timeout or unexpected response. Expected: " + expectedResponse);
  }
  return false;
}

void sendRaw(const uint8_t* buffer, size_t size) {
    for(size_t i = 0; i < size; i++) {
        modem.write(buffer[i]);
    }
}


bool setupSIM7600() {
  Serial.println("[INFO] Initializing SIM7600G...");

  if (!sendATCommand("AT", "OK", 3000)) return false;
  if (!sendATCommand("ATE0", "OK", 3000)) return false; 
  if (!sendATCommand("AT+CPIN?", "READY", 5000)) return false;
  
  Serial.println("[INFO] Waiting for network registration...");
  unsigned long regStart = millis();
  while (millis() - regStart < 60000) {
    if (sendATCommand("AT+CGREG?", "+CGREG: 0,1", 2000, false) || sendATCommand("AT+CGREG?", "+CGREG: 0,5", 2000, false)) {
      Serial.println("[INFO] Registered to network.");
      break;
    }
    Serial.println("[INFO] Still waiting for network registration...");
    delay(2000);
  }
  if (millis() - regStart >= 60000) {
    Serial.println("[ERROR] Network registration timeout.");
    return false;
  }
  sendATCommand("AT+CGREG?", "OK", 2000); // Print final registration status

  String cmd = "AT+CGDCONT=1,\"IP\",\"" + apn + "\"";
  if (!sendATCommand(cmd, "OK", 5000)) return false;
  
  // Enable HTTP/S
  if (!sendATCommand("AT+HTTPINIT", "OK", 5000)) {
     Serial.println("[WARN] HTTP already initialized. Continuing...");
     sendATCommand("AT+HTTPTERM", "OK", 5000);
     delay(1000);
     if (!sendATCommand("AT+HTTPINIT", "OK", 5000)) {
        Serial.println("[ERROR] Failed to initialize HTTP.");
        return false;
     }
  }

  Serial.println("[INFO] SIM7600G initialized successfully.");
  return true;
}

void listWavFiles() {
  Serial.println("[INFO] Listing .wav files on SD card...");
  File root = SD.open("/");
  if (!root) {
    Serial.println("[ERROR] Failed to open root directory.");
    return;
  }
  if (!root.isDirectory()) {
    Serial.println("[ERROR] Root is not a directory.");
    root.close();
    return;
  }

  fileCount = 0;
  File file = root.openNextFile();
  while (file) {
    String name = file.name();
    if ((name.endsWith(".wav") || name.endsWith(".WAV")) && name.length() > 1 && name[0] != '_') {
      if (fileCount < MAX_FILES) {
        fileList[fileCount++] = "/" + name;
      }
    }
    file.close();
    file = root.openNextFile();
  }
  root.close();

  if (fileCount == 0) {
    Serial.println("[INFO] No .wav files found.");
  } else {
    Serial.println("[INFO] Found .wav files:");
    for (int i = 0; i < fileCount; i++) {
      Serial.println(String(i + 1) + ": " + fileList[i]);
    }
  }
}

bool uploadFile(String filename) {
    File file = SD.open(filename);
    if (!file) {
        Serial.println("[ERROR] Failed to open file: " + filename);
        return false;
    }

    String boundary = "----WebKitFormBoundary7MA4YWxkTrZu0gW";
    String fileIdentifier = filename + "-" + String(file.size());
    size_t totalChunks = (file.size() + 512 - 1) / 512;
    
    Serial.println("[INFO] Uploading " + filename + " in " + totalChunks + " chunks.");

    for (size_t i = 0; i < totalChunks; i++) {
        uint8_t buffer[512];
        size_t chunkSize = file.read(buffer, sizeof(buffer));

        if (chunkSize == 0) {
            break; 
        }

        String start_body = "--" + boundary + "\r\n";
        start_body += "Content-Disposition: form-data; name=\"chunk\"; filename=\"" + filename.substring(1) + "\"\r\n";
        start_body += "Content-Type: application/octet-stream\r\n\r\n";
        
        String end_body = "\r\n--" + boundary + "\r\n";
        end_body += "Content-Disposition: form-data; name=\"fileIdentifier\"\r\n\r\n" + fileIdentifier + "\r\n";
        end_body += "--" + boundary + "\r\n";
        end_body += "Content-Disposition: form-data; name=\"chunkIndex\"\r\n\r\n" + String(i) + "\r\n";
        end_body += "--" + boundary + "\r\n";
        end_body += "Content-Disposition: form-data; name=\"totalChunks\"\r\n\r\n" + String(totalChunks) + "\r\n";
        end_body += "--" + boundary + "\r\n";
        end_body += "Content-Disposition: form-data; name=\"originalFilename\"\r\n\r\n" + filename.substring(1) + "\r\n";
        end_body += "--" + boundary + "--\r\n";

        size_t total_len = start_body.length() + chunkSize + end_body.length();

        Serial.println("[INFO] Uploading chunk " + String(i + 1) + "/" + String(totalChunks) + ", size: " + String(chunkSize) + " bytes");
        
        if (!sendATCommand("AT+HTTPPARA=\"URL\",\"http://" + host + path + "\"", "OK", 5000)) return false;
        if (!sendATCommand("AT+HTTPPARA=\"CONTENT\",\"multipart/form-data; boundary=" + boundary + "\"", "OK", 5000)) return false;

        String data_cmd = "AT+HTTPDATA=" + String(total_len) + ",20000"; // Set total length and 20s timeout
        if (!sendATCommand(data_cmd, "DOWNLOAD", 5000)) {
            Serial.println("[ERROR] Failed to start data send.");
            return false;
        }

        modem.print(start_body);
        sendRaw(buffer, chunkSize);
        modem.print(end_body);

        if (!sendATCommand("", "OK", 20000, false)) { // Wait for OK after data is sent
           Serial.println("[WARN] Did not receive OK after chunk. Checking action response...");
        }

        if (!sendATCommand("AT+HTTPACTION=1", "+HTTPACTION: 1,200", 20000)) {
             Serial.println("[ERROR] POST action failed for chunk " + String(i+1));
             sendATCommand("AT+HTTPREAD", "OK", 10000); // Read response body to debug
             return false;
        }
        Serial.println("[INFO] Chunk " + String(i + 1) + " uploaded successfully.");
    }

    file.close();
    Serial.println("[INFO] File upload complete.");
    return true;
}

void setup() {
  Serial.begin(115200);
  while (!Serial); 
  delay(1000);
  Serial.println("\n=== Cellular Data Streamer ===");

  modem.begin(115200);
  Serial.println("[INFO] Modem serial started.");

  Serial.println("[INFO] Initializing SD card...");
  if (!SD.begin(SD_CS)) {
    Serial.println("[ERROR] SD card init failed!");
    while (1) delay(1000);
  }
  Serial.println("[INFO] SD card ready.");

  listWavFiles();

  if (fileCount == 0) {
    Serial.println("[INFO] No .wav files found. Please ensure files are in the root directory.");
    Serial.println("[INFO] Halting.");
    while (1) delay(1000);
  }

  int selectedIndex = -1;
  while (selectedIndex < 0) {
    Serial.println("\nSelect file number to upload (or type 0 to refresh list):");
    while (!Serial.available()) {
      delay(100);
    }
    String input = Serial.readStringUntil('\n');
    input.trim();
    int num = input.toInt();

    if (num == 0) {
      listWavFiles();
      continue;
    }

    if (num >= 1 && num <= fileCount) {
      selectedIndex = num - 1;
    } else {
      Serial.println("Invalid selection, try again.");
    }
  }

  Serial.println("[INFO] Selected file: " + fileList[selectedIndex]);

  if (!setupSIM7600()) {
    Serial.println("[FATAL] SIM7600 initialization failed. Halting.");
    while (1) delay(1000);
  }

  if (!uploadFile(fileList[selectedIndex])) {
    Serial.println("[ERROR] Upload failed.");
  } else {
    Serial.println("[SUCCESS] Upload succeeded!");
  }
  
  sendATCommand("AT+HTTPTERM", "OK", 5000);

  Serial.println("\n[INFO] Process complete. Restarting in 10 seconds...");
  delay(10000);
  ESP.restart();
}

void loop() {
  // Nothing to do here
}
