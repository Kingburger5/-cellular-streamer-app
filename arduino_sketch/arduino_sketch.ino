#include <HardwareSerial.h>
#include <SPI.h>
#include <SD.h>

// SIM7600 serial pins
#define MODEM_RX 16
#define MODEM_TX 17
#define MODEM_BAUD 115200

#define SD_CS 5  // Change to your SD CS pin

HardwareSerial modem(2);  // UART2 on ESP32

// APN for your carrier
const char apn[] = "vodafone";

// Server settings
const char* host = "cellular-data-streamer.web.app";
const char* path = "/api/upload"; // Corrected path

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("Initializing SD card...");
  if (!SD.begin(SD_CS)) {
    Serial.println("❌ SD card initialization failed!");
    while (true);
  }
  Serial.println("✅ SD card ready.");

  Serial.println("Listing files on SD card:");
  listFiles(SD, "/");

  Serial.println("\nEnter filename to upload (including leading /): ");
  String filename = "";
  while (filename.length() == 0) {
    if (Serial.available()) {
      filename = Serial.readStringUntil('\n');
      filename.trim();
    }
  }

  if (!SD.exists(filename)) {
    Serial.println("❌ File not found: " + filename);
    while (true);
  }

  // Initialize modem UART
  modem.begin(MODEM_BAUD, SERIAL_8N1, MODEM_RX, MODEM_TX);
  delay(3000);

  Serial.println("Initializing modem...");
  if (!sendATCommand("AT", "OK", 3000)) {
    Serial.println("❌ Modem not responding");
    while (true);
  }

  if (!sendATCommand("ATE0", "OK", 3000)) {  // Disable echo
    Serial.println("❌ Failed to disable echo");
  }

  if (!sendATCommand("AT+CPIN?", "READY", 10000)) {
    Serial.println("❌ SIM not ready");
    while (true);
  }

  Serial.println("Connecting to network...");
  unsigned long startNetwork = millis();
  bool registered = false;
  while (millis() - startNetwork < 60000) {  // 60 sec timeout
    if (sendATCommand("AT+CGREG?", "+CGREG: 0,1", 5000) ||
        sendATCommand("AT+CGREG?", "+CGREG: 0,5", 5000)) {
      registered = true;
      break;
    }
    Serial.print(".");
    delay(2000);
  }
  if (!registered) {
    Serial.println("\n❌ Network registration failed");
    while (true);
  }
  Serial.println("\n✅ Network registered");

  if (!sendATCommand(String("AT+CGDCONT=1,\"IP\",\"") + apn + "\"", "OK", 5000)) {
    Serial.println("❌ Failed to set APN");
    while (true);
  }

  if (!sendATCommand("AT+CGACT=1,1", "OK", 10000)) {
    Serial.println("❌ Failed to activate PDP context");
    while (true);
  }

  Serial.println("GPRS connected.");

  bool uploadSuccess = uploadFile(filename);

  if (uploadSuccess) Serial.println("✅ File uploaded successfully!");
  else Serial.println("❌ File upload failed!");
}

void loop() {
  // nothing here
}

// List files on SD card (non-recursive)
void listFiles(fs::FS &fs, const char *dirname) {
  File root = fs.open(dirname);
  if (!root || !root.isDirectory()) {
    Serial.println("Failed to open directory");
    return;
  }
  File file = root.openNextFile();
  while (file) {
    if (!file.isDirectory()) {
      Serial.print("  • ");
      Serial.print(file.name());
      Serial.print(" (");
      Serial.print(file.size());
      Serial.println(" bytes)");
    }
    file = root.openNextFile();
  }
}

// Send AT command and wait for expected response
bool sendATCommand(String cmd, String expect, int timeout) {
  unsigned long start = millis();
  String resp = "";
  
  while (modem.available()) modem.read();  // Clear buffer before sending
  
  modem.print(cmd + "\r\n");
  Serial.print(">> ");
  Serial.println(cmd);

  while (millis() - start < (unsigned long)timeout) {
    while (modem.available()) {
      char c = modem.read();
      resp += c;
    }
    if (resp.indexOf(expect) != -1) {
      Serial.println(resp);
      return true;
    }
    if (resp.indexOf("ERROR") != -1) {
      Serial.println(resp);
      return false;
    }
  }
  Serial.println("[ERROR] Timeout waiting for: " + expect);
  Serial.println("[RESPONSE] " + resp);
  return false;
}

// Upload file in binary chunks with custom headers
bool uploadFile(const String &filename) {
  File file = SD.open(filename, FILE_READ);
  if (!file) {
    Serial.println("Failed to open file");
    return false;
  }
  size_t fileSize = file.size();
  const size_t CHUNK_SIZE = 1024;  // Max chunk size safe for SIM7600
  int totalChunks = (fileSize + CHUNK_SIZE - 1) / CHUNK_SIZE;
  String fileId = filename + "-" + String(fileSize);

  Serial.printf("Uploading %s, size %d bytes in %d chunks\n", filename.c_str(), fileSize, totalChunks);

  for (int i = 0; i < totalChunks; i++) {
    size_t offset = i * CHUNK_SIZE;
    size_t chunkSize = min(CHUNK_SIZE, fileSize - offset);
    file.seek(offset);

    // Prepare HTTP session
    if (!sendATCommand("AT+HTTPTERM", "OK", 3000)) {
      Serial.println("Warning: HTTPTERM failed, continuing");
    }
    delay(300);
    if (!sendATCommand("AT+HTTPINIT", "OK", 5000)) {
      Serial.println("HTTPINIT failed");
      file.close();
      return false;
    }
    delay(300);
    if (!sendATCommand("AT+HTTPPARA=\"CID\",1", "OK", 3000)) {
      Serial.println("Failed to set HTTP CID");
      file.close();
      return false;
    }
    if (!sendATCommand("AT+HTTPPARA=\"URL\",\"http://" + String(host) + path + "\"", "OK", 5000)) {
      Serial.println("Failed to set HTTP URL");
      file.close();
      return false;
    }
    // Custom headers (escaped)
    String headers = "X-File-ID: " + fileId + "\\r\\n" +
                     "X-Chunk-Index: " + String(i) + "\\r\\n" +
                     "X-Total-Chunks: " + String(totalChunks) + "\\r\\n" +
                     "X-Original-Filename: " + filename; // Corrected header name
    
    if (!sendATCommand("AT+HTTPPARA=\"USERDATA\",\"" + headers + "\"", "OK", 3000)) {
      Serial.println("Failed to set HTTP headers");
      file.close();
      return false;
    }
    if (!sendATCommand("AT+HTTPPARA=\"CONTENT\",\"application/octet-stream\"", "OK", 3000)) {
      Serial.println("Failed to set content type");
      file.close();
      return false;
    }

    // Send chunk data
    if (!sendATCommand("AT+HTTPDATA=" + String(chunkSize) + ",10000", "DOWNLOAD", 5000)) {
      Serial.println("HTTPDATA failed");
      file.close();
      return false;
    }

    uint8_t buf[CHUNK_SIZE];
    size_t bytesRead = file.read(buf, chunkSize);
    modem.write(buf, bytesRead);

    // Execute HTTP POST
    if (!sendATCommand("AT+HTTPACTION=1", "+HTTPACTION: 1,", 15000)) {
      Serial.println("Chunk upload failed at index " + String(i));
      file.close();
      return false;
    }

    Serial.printf("Uploaded chunk %d/%d\n", i + 1, totalChunks);
    delay(1000);  // Cooldown for modem stability
  }

  file.close();
  return true;
}
