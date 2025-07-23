#define TINY_GSM_MODEM_SIM7600

#include <HardwareSerial.h>
#include <TinyGsmClient.h>
#include <SD.h>
#include <SPI.h>

// Your GPRS credentials
const char apn[] = "vodafone";
const char gprsUser[] = "";
const char gprsPass[] = "";

// Server details
const char server[] = "9000-firebase-studio-1753223410587.cluster-73qgvk7hjjadkrjeyexca5ivva.cloudworkstations.dev";
const char resource[] = "/api/upload";
const int port = 9000;

#define MODEM_TX 17
#define MODEM_RX 16
#define MODEM_BAUD 115200
#define SD_CS 5
#define CHUNK_SIZE 4096 // 4KB chunks
#define MAX_RETRIES 3
#define RETRY_DELAY_MS 5000

HardwareSerial simSerial(1);
TinyGsm modem(simSerial);
TinyGsmClient client(modem);

void debugModemStatus() {
  Serial.println("--- Modem Status ---");
  Serial.print("Modem Info: ");
  Serial.println(modem.getModemInfo());

  Serial.print("Signal Quality: ");
  Serial.println(modem.getSignalQuality());

  Serial.print("SIM Status: ");
  if (modem.getSimStatus() != SIM_READY) {
    Serial.println("SIM not ready");
  } else {
    Serial.println("SIM ready");
    Serial.print("CCID: ");
    Serial.println(modem.getSimCCID());
  }

  Serial.print("Operator: ");
  Serial.println(modem.getOperator());
  Serial.println("--------------------");
}


void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("🔌 Booting...");

  if (!SD.begin(SD_CS)) {
    Serial.println("❌ SD card failed.");
    while (true);
  }
  Serial.println("✅ SD card ready.");

  Serial.println("Initializing modem...");
  simSerial.begin(MODEM_BAUD, SERIAL_8N1, MODEM_RX, MODEM_TX);
  delay(6000); // Wait for modem to stabilize
  
  if (!modem.restart()) {
    Serial.println("❌ Failed to restart modem");
    return;
  }
  Serial.println("✅ Modem restarted.");

  debugModemStatus();

  Serial.println("📡 Connecting to network...");
  if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
    Serial.println("❌ Failed to connect to GPRS.");
    return;
  }

  Serial.println("✅ GPRS connected.");
  Serial.print("Local IP: ");
  Serial.println(modem.getLocalIP());

  // Upload your file here
  sendFileChunks("/sigma2.wav");
}

void loop() {
  // Nothing to do here
}

void sendFileChunks(const char* filename) {
  File file = SD.open(filename, FILE_READ);
  if (!file) {
    Serial.printf("❌ Failed to open file: %s\n", filename);
    return;
  }

  size_t totalSize = file.size();
  Serial.printf("📤 Preparing to upload %s (%u bytes)\n", filename, totalSize);

  size_t offset = 0;
  while (offset < totalSize) {
    size_t chunkSize = min((size_t)CHUNK_SIZE, totalSize - offset);

    bool success = false;
    for (int retry = 0; retry < MAX_RETRIES; retry++) {
      Serial.printf("  Attempt %d/%d to send chunk at offset %u...\n", retry + 1, MAX_RETRIES, offset);

      if (!client.connect(server, port)) {
        Serial.printf("    ...connection to %s:%d failed.\n", server, port);
        delay(RETRY_DELAY_MS);
        continue;
      }
      Serial.println("    ...connected to server.");
      
      // Send HTTP POST request header
      client.print(String("POST ") + resource + " HTTP/1.1\r\n");
      client.print(String("Host: ") + server + "\r\n");
      
      String sanitizedFilename = String(filename);
      if (sanitizedFilename.startsWith("/")) {
        sanitizedFilename = sanitizedFilename.substring(1);
      }
      
      client.print(String("X-Filename: ") + sanitizedFilename + "\r\n");
      client.print(String("X-Chunk-Offset: ") + offset + "\r\n");
      client.print(String("X-Chunk-Size: ") + chunkSize + "\r\n");
      client.print(String("X-Total-Size: ") + totalSize + "\r\n");
      
      client.print(String("Content-Length: ") + chunkSize + "\r\n");
      client.print("Connection: close\r\n");
      client.print("\r\n");

      // Send the chunk data
      uint8_t buffer[256];
      size_t bytesRead = 0;
      file.seek(offset);
      while (bytesRead < chunkSize) {
          size_t toRead = min((size_t)sizeof(buffer), chunkSize - bytesRead);
          size_t readNow = file.read(buffer, toRead);
          client.write(buffer, readNow);
          bytesRead += readNow;
      }
      
      // Wait for server response
      unsigned long timeout = millis();
      bool serverOk = false;
      Serial.println("--- Server Response ---");
      while (millis() - timeout < 10000) {
        while (client.available()) {
          String line = client.readStringUntil('\n');
          line.trim();
          Serial.println(line);
          if (line.startsWith("HTTP/1.1 200") || line.startsWith("HTTP/1.1 201")) {
            serverOk = true;
          }
        }
        if (serverOk) break;
      }
      Serial.println("-----------------------");

      client.stop();
      Serial.println("    ...connection closed.");


      if (serverOk) {
        Serial.printf("✅ Chunk at offset %u sent successfully.\n", offset);
        success = true;
        break; // Exit retry loop
      } else {
        Serial.printf("❌ Server responded with an error for chunk at offset %u.\n", offset);
        delay(RETRY_DELAY_MS);
      }
    }

    if (!success) {
      Serial.printf("❌ Failed to upload chunk at offset %u after %d retries. Aborting.\n", offset, MAX_RETRIES);
      break; // Exit main while loop
    }

    offset += chunkSize;
  }

  file.close();
  if (offset == totalSize) {
    Serial.println("✅ File upload completed successfully!");
  } else {
    Serial.println("❌ File upload failed.");
  }
}
