#include <SPI.h>
#include <SD.h>
#include <HardwareSerial.h>

// SIM7600 Serial pins
#define SIM7600_TX 17 // ESP32 TX to SIM7600 RX
#define SIM7600_RX 16 // ESP32 RX to SIM7600 TX
#define SD_CS      5  // SD card CS pin

HardwareSerial modem(2); // Use UART2 for the modem

// APN and server config
String apn = "internet";
String host = "cellular-data-streamer.web.app";
String path = "/api/upload";

#define MAX_FILES 50
String fileList[MAX_FILES];
int fileCount = 0;

// Helper to send AT command and wait for expected response
bool sendATCommand(String cmd, String expectedResponse, unsigned long timeout) {
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
      // Don't print byte by byte, it's too slow.
    }
     if (response.indexOf(expectedResponse) != -1) {
        Serial.print("[MODEM] ");
        Serial.println(response);
        return true;
      }
      if (response.indexOf("ERROR") != -1) {
        Serial.print("[MODEM] ");
        Serial.println(response);
        return false;
      }
  }

  Serial.println("[ERROR] Timeout or unexpected response.");
  Serial.print("[MODEM] ");
  Serial.println(response);
  return false;
}


bool setupSIM7600() {
  Serial.println("[INFO] Initializing SIM7600G...");

  if (!sendATCommand("AT", "OK", 3000)) return false;
  if (!sendATCommand("ATE0", "OK", 3000)) return false; // Disable echo
  if (!sendATCommand("AT+CPIN?", "READY", 5000)) return false;
  if (!sendATCommand("AT+CSQ", "OK", 3000)) return false;

  Serial.println("[INFO] Waiting for network registration...");
  unsigned long regStart = millis();
  while (millis() - regStart < 60000) {
    if (sendATCommand("AT+CGREG?", "+CGREG: 0,1", 5000) ||
        sendATCommand("AT+CGREG?", "+CGREG: 0,5", 5000)) {
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

  if (!sendATCommand("AT+COPS?", "OK", 5000)) return false;

  String cmd = "AT+CGDCONT=1,\"IP\",\"" + apn + "\"";
  if (!sendATCommand(cmd, "OK", 5000)) return false;
  
  // Enable HTTP(S) stack
  if (!sendATCommand("AT+HTTPINIT", "OK", 5000)) {
      Serial.println("[WARN] HTTP already initialized.");
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
    if (name.endsWith(".wav") || name.endsWith(".WAV")) {
      if (fileCount < MAX_FILES) {
        fileList[fileCount++] = name;
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

// Re-implementation of uploadFile to be compatible with the server
bool uploadFile(String filename) {
    File file = SD.open(filename, FILE_READ);
    if (!file) {
        Serial.println("[ERROR] Failed to open file for upload.");
        return false;
    }

    const size_t CHUNK_SIZE = 1024 * 4; // 4KB chunks
    size_t fileSize = file.size();
    int totalChunks = (fileSize + CHUNK_SIZE - 1) / CHUNK_SIZE;
    String fileIdentifier = filename + "-" + String(fileSize) + "-" + String(file.getLastWrite());
    
    Serial.println("[INFO] Starting upload for: " + filename);
    Serial.println("[INFO] File Size: " + String(fileSize) + " bytes, Total Chunks: " + String(totalChunks));

    for (int chunkIndex = 0; chunkIndex < totalChunks; chunkIndex++) {
        Serial.println("\n[INFO] Uploading chunk " + String(chunkIndex + 1) + "/" + String(totalChunks));

        size_t startByte = chunkIndex * CHUNK_SIZE;
        size_t endByte = min(startByte + CHUNK_SIZE, fileSize);
        size_t currentChunkSize = endByte - startByte;

        file.seek(startByte);
        
        String boundary = "----WebKitFormBoundary7MA4YWxkTrZu0gW";
        
        // Construct the multipart/form-data body
        String data_start = "--" + boundary + "\r\n";
        data_start += "Content-Disposition: form-data; name=\"chunk\"; filename=\"" + filename + "\"\r\n";
        data_start += "Content-Type: application/octet-stream\r\n\r\n";
        
        String data_end = "\r\n--" + boundary + "\r\n";
        data_end += "Content-Disposition: form-data; name=\"fileIdentifier\"\r\n\r\n" + fileIdentifier + "\r\n";
        data_end += "--" + boundary + "\r\n";
        data_end += "Content-Disposition: form-data; name=\"chunkIndex\"\r\n\r\n" + String(chunkIndex) + "\r\n";
        data_end += "--" + boundary + "\r\n";
        data_end += "Content-Disposition: form-data; name=\"totalChunks\"\r\n\r\n" + String(totalChunks) + "\r\n";
        data_end += "--" + boundary + "\r\n";
        data_end += "Content-Disposition: form-data; name=\"originalFilename\"\r\n\r\n" + filename + "\r\n";
        data_end += "--" + boundary + "--\r\n";

        size_t totalPayloadSize = data_start.length() + currentChunkSize + data_end.length();

        // 1. Set URL
        if (!sendATCommand("AT+HTTPPARA=\"URL\",\"http://" + host + path + "\"", "OK", 5000)) return false;
        
        // 2. Set Content-Type
        if (!sendATCommand("AT+HTTPPARA=\"CONTENT\",\"multipart/form-data; boundary=" + boundary + "\"", "OK", 5000)) return false;

        // 3. Set data size
        if (!sendATCommand("AT+HTTPDATA=" + String(totalPayloadSize) + ",120000", "DOWNLOAD", 10000)) {
            Serial.println("[ERROR] Failed to start HTTP data command.");
            return false;
        }

        // 4. Send data
        Serial.println("[INFO] Sending payload...");
        modem.print(data_start);
        
        uint8_t buffer[256];
        size_t bytesSentForChunk = 0;
        while(bytesSentForChunk < currentChunkSize) {
            size_t willRead = min((size_t)sizeof(buffer), currentChunkSize - bytesSentForChunk);
            size_t didRead = file.read(buffer, willRead);
            modem.write(buffer, didRead);
            bytesSentForChunk += didRead;
        }
        
        modem.print(data_end);
        
        // Wait for modem to confirm data receipt
        if (!sendATCommand("", "OK", 20000)) { // Wait for the OK after data sending
          Serial.println("[ERROR] Modem did not confirm data receipt.");
          return false;
        }

        // 5. POST action
        if (!sendATCommand("AT+HTTPACTION=1", "OK", 20000)) return false; // 1 for POST

        // 6. Check response
        unsigned long readStart = millis();
        bool success = false;
        while(millis() - readStart < 20000) {
           if(modem.find("+HTTPACTION: 1,200")) { // 1=POST, 200=OK
              Serial.println("[INFO] Chunk uploaded successfully.");
              success = true;
              break;
           }
        }

        if (!success) {
            Serial.println("[ERROR] Server did not return 200 OK.");
            // Optional: read server response
            sendATCommand("AT+HTTPREAD", "", 5000);
            return false;
        }
    }
    
    file.close();
    Serial.println("[INFO] File upload finished successfully!");
    return true;
}


void setup() {
  Serial.begin(115200);
  delay(1000);
  while (!Serial); // Wait for serial port to connect
  
  Serial.println("\n=== Cellular Data Streamer v2.0 ===");

  modem.begin(115200);
  Serial.println("[INFO] Modem serial started.");

  Serial.println("[INFO] Initializing SD card...");
  if (!SD.begin(SD_CS)) {
    Serial.println("[ERROR] SD card init failed! Halting.");
    while (1) delay(1000);
  }
  Serial.println("[INFO] SD card ready.");

  listWavFiles();

  if (fileCount == 0) {
    Serial.println("[INFO] No wav files found. Waiting for new files...");
    // You could loop here waiting for files
    while (1) delay(1000);
  }

  int selectedIndex = -1;
  while (selectedIndex < 0) {
    Serial.println("\nSelect file number to upload (or type 0 to re-list files):");
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
      Serial.println("[WARN] Invalid selection, please try again.");
    }
  }

  String selectedFilename = fileList[selectedIndex];
  Serial.println("[INFO] Selected file: " + selectedFilename);

  if (!setupSIM7600()) {
    Serial.println("[FATAL] SIM7600 initialization failed. Halting.");
    while (1) delay(1000);
  }

  if (!uploadFile(selectedFilename)) {
    Serial.println("\n[FATAL] Upload failed. Halting.");
  } else {
    Serial.println("\n[SUCCESS] Upload complete!");
  }
   
  // Optional: Terminate HTTP after use
  sendATCommand("AT+HTTPTERM", "OK", 5000);
  
  Serial.println("[INFO] Process finished.");
}

void loop() {
  // Let ESP32 sleep or idle
  delay(10000);
}
