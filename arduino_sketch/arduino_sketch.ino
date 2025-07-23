#define TINY_GSM_MODEM_SIM7600
#include <TinyGsmClient.h>
#include <SPI.h>
#include <SD.h>
#include <HardwareSerial.h>

// Set serial for debug console (to Serial Monitor, default speed 115200)
#define SerialMon Serial
// Set serial for AT commands (to SIM7600 module)
#define SerialAT Serial1

// Configure SIM7600 modem
#define MODEM_RX 16
#define MODEM_TX 17
#define MODEM_BAUD 115200

// SD card chip select pin
#define SD_CS 5

// Network details
const char apn[] = "vodafone"; // Your APN
const char gprsUser[] = "";
const char gprsPass[] = "";

// Server details
const char server[] = "9000-firebase-studio-1753223410587.cluster-73qgvk7hjjadkrjeyexca5ivva.cloudworkstations.dev";
const char resource[] = "/api/upload";
const int port = 443; // Use 443 for HTTPS

// File upload settings
const size_t CHUNK_SIZE = 1024 * 5; // 5 KB chunk size
const int MAX_RETRIES = 3;

// TinyGSM client for cellular connection
TinyGsm modem(SerialAT);
// Use TinyGsmClient for HTTP or TinyGsmClientSecure for HTTPS
#include <TinyGsmClientSecure.h>
TinyGsmClientSecure client(modem);

void setup() {
  SerialMon.begin(115200);
  delay(10);

  SerialMon.println("🔌 Booting...");

  // Initialize SD card
  if (!SD.begin(SD_CS)) {
    SerialMon.println("❌ SD card initialization failed!");
    while (true);
  }
  SerialMon.println("✅ SD card ready.");
  
  // Set modem baud rate and start
  SerialAT.begin(MODEM_BAUD, SERIAL_8N1, MODEM_RX, MODEM_TX);
  delay(6000);
  
  SerialMon.println("Initializing modem...");
  if (!modem.init()) {
    SerialMon.println("❌ Failed to initialize modem");
    return;
  }
  
  String modemInfo;
  modem.getModemInfo(modemInfo);
  SerialMon.print("Modem Info: ");
  SerialMon.println(modemInfo);

  SerialMon.println("📡 Connecting to network...");
  if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
    SerialMon.println("❌ GPRS connect failed");
    return;
  }
  
  unsigned long start = millis();
  while (!modem.isGprsConnected() && (millis() - start < 30000)) {
    SerialMon.print(".");
    delay(1000);
  }
  if (!modem.isGprsConnected()) {
    SerialMon.println("\n❌ Failed to connect to GPRS within 30 seconds.");
    while (true);
  }

  SerialMon.println("\n✅ GPRS status: connected");
  SerialMon.print("SIM CCID: ");
  SerialMon.println(modem.getSimCCID());
  SerialMon.print("IMEI: ");
  SerialMon.println(modem.getIMEI());
  SerialMon.print("Operator: ");
  SerialMon.println(modem.getOperator());
  SerialMon.print("Local IP: ");
  SerialMon.println(modem.getLocalIP());

  SerialMon.println("\n✅ Modem and network ready.");

  // For HTTPS, you may need to set the root CA certificate if the server uses a self-signed cert
  // For this example, we will proceed without certificate validation for simplicity (less secure)
  client.setInsecure();

  // --- Start Upload ---
  const char* filename = "/sigma2.wav"; // The file to upload
  uploadFile(filename);
}

void loop() {
  // Keep the connection alive
  modem.maintain();
}

void uploadFile(const char* filename) {
  File file = SD.open(filename, FILE_READ);
  if (!file) {
    SerialMon.printf("❌ Failed to open file %s for reading\n", filename);
    return;
  }

  size_t totalSize = file.size();
  size_t totalChunks = (totalSize + CHUNK_SIZE - 1) / CHUNK_SIZE;
  SerialMon.printf("📤 Uploading %s (%u bytes) in %u chunks.\n", filename, totalSize, totalChunks);

  size_t offset = 0;
  for (int chunkIndex = 0; offset < totalSize; chunkIndex++) {
    int retries = 0;
    bool chunk_ok = false;
    
    while(retries < MAX_RETRIES && !chunk_ok) {
      if (retries > 0) {
        SerialMon.printf("   Retrying chunk %d... (Attempt %d/%d)\n", chunkIndex + 1, retries + 1, MAX_RETRIES);
      }

      SerialMon.printf("📤 Uploading chunk %d/%u (%d bytes)\n", chunkIndex + 1, totalChunks, min((size_t)CHUNK_SIZE, totalSize - offset));
      
      // Establish connection
      SerialMon.printf("Connecting to %s...", server);
      if (!client.connect(server, port)) {
        SerialMon.println(" ❌ connection failed.");
        retries++;
        delay(2000);
        continue;
      }
      SerialMon.println(" ✅");

      // Build and send headers
      client.print(String("POST ") + resource + " HTTP/1.1\r\n");
      client.print(String("Host: ") + server + "\r\n");
      client.print("Connection: close\r\n");
      // Custom headers for our server
      client.print(String("x-filename: ") + (strrchr(filename, '/') ? strrchr(filename, '/') + 1 : filename) + "\r\n");
      client.print(String("x-chunk-offset: ") + String(offset) + "\r\n");
      client.print(String("x-chunk-size: ") + String(min(CHUNK_SIZE, totalSize - offset)) + "\r\n");
      client.print(String("x-total-size: ") + String(totalSize) + "\r\n");
      // Standard headers
      client.print(String("Content-Length: ") + String(min(CHUNK_SIZE, totalSize - offset)) + "\r\n");
      client.print("Content-Type: application/octet-stream\r\n");
      client.print("\r\n");

      // Send the chunk data
      size_t bytesSent = 0;
      uint8_t buffer[256];
      file.seek(offset);
      while(bytesSent < min(CHUNK_SIZE, totalSize - offset)) {
          int bytesToRead = file.read(buffer, sizeof(buffer));
          if (bytesToRead <= 0) break;
          client.write(buffer, bytesToRead);
          bytesSent += bytesToRead;
      }
      client.flush();

      // Wait for server response
      unsigned long timeout = millis();
      String headers = "";
      String body = "";
      bool reading_body = false;

      while (client.connected() && millis() - timeout < 15000) {
        if (client.available()) {
          char c = client.read();
          if (reading_body) {
            body += c;
          } else {
            headers += c;
            if (headers.endsWith("\r\n\r\n")) {
              reading_body = true;
            }
          }
        }
      }

      client.stop(); // Disconnect after each chunk

      SerialMon.println("--- SERVER RESPONSE ---");
      SerialMon.println(headers);
      SerialMon.println(body);
      SerialMon.println("-----------------------");

      if (headers.indexOf("HTTP/1.1 200 OK") >= 0 || headers.indexOf("HTTP/1.1 201 Created") >= 0) {
        chunk_ok = true;
      } else {
        SerialMon.println("❌ Chunk upload failed. Server responded with error.");
        retries++;
        delay(2000);
      }
    }
    
    if (!chunk_ok) {
        SerialMon.printf("❌ Failed to upload chunk %d after %d retries. Aborting.\n", chunkIndex + 1, MAX_RETRIES);
        file.close();
        return;
    }

    offset += CHUNK_SIZE;
  }

  SerialMon.println("✅ File upload complete.");
  file.close();
}
