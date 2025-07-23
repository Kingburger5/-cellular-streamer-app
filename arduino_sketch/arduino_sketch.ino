
#define TINY_GSM_MODEM_SIM7600

#include <HardwareSerial.h>
#include <TinyGsmClient.h>
#include <SD.h>
#include <SPI.h>

// ===== Configuration =====
const char* server = "cellular-data-streamer.web.app";
const int serverPort = 443;
const char* apn = "vodafone"; // Your APN
const char* gprsUser = "";    // Your GPRS user, if any
const char* gprsPass = "";    // Your GPRS password, if any

#define MODEM_TX 17
#define MODEM_RX 16
#define MODEM_BAUD 115200
#define SD_CS 5
#define CHUNK_SIZE 4096 // Keep chunk size reasonable for AT command buffer

// ===== Globals =====
HardwareSerial modemSerial(1);
TinyGsm modem(modemSerial);

// ===== Helper Functions =====

/**
 * @brief Reads the modem's response until a specific string is found or timeout.
 * @param target The string to wait for (e.g., "OK", ">").
 * @param timeout Duration to wait in milliseconds.
 * @return True if the target string is found, false otherwise.
 */
bool expectResponse(const String& target, unsigned long timeout) {
  String buffer;
  unsigned long start = millis();
  while (millis() - start < timeout) {
    if (modemSerial.available()) {
      char c = modemSerial.read();
      buffer += c;
      Serial.write(c); // Echo response to Serial for debugging
      if (buffer.indexOf(target) != -1) {
        return true;
      }
    }
  }
  Serial.println("\n[DEBUG] Timeout waiting for: " + target);
  Serial.println("[DEBUG] Received: " + buffer);
  return false;
}

/**
 * @brief Sends an AT command and waits for "OK".
 * @param cmd The AT command to send.
 * @param timeout Duration to wait for "OK".
 * @return True if "OK" is received, false otherwise.
 */
bool sendAT(const String& cmd, unsigned long timeout = 1000) {
    Serial.println("[AT SEND] " + cmd);
    modemSerial.println(cmd);
    return expectResponse("OK", timeout);
}


void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("🔌 Booting...");

  if (!SD.begin(SD_CS)) {
    Serial.println("❌ SD card failed to initialize. Halting.");
    while (true);
  }
  Serial.println("✅ SD card ready.");

  Serial.println("Initializing modem...");
  modemSerial.begin(MODEM_BAUD, SERIAL_8N1, MODEM_RX, MODEM_TX);
  delay(3000);
  modem.restart();
  
  Serial.println("✅ Modem restarted.");

  Serial.println("--- Modem Status ---");
  sendAT("ATI", 1000);
  sendAT("AT+CSQ", 1000);
  sendAT("AT+CPIN?", 1000);
  sendAT("AT+CREG?", 1000);
  Serial.println("--------------------");

  Serial.println("📡 Connecting to network...");
  if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
      Serial.println("❌ GPRS connection failed.");
      while(true);
  }
  
  Serial.println("✅ GPRS connected.");
  sendAT("AT+IPADDR", 1000);


  // Upload the file
  sendFileChunks("/sigma2.wav");
}

void loop() {
  // Keep empty
}


void sendFileChunks(const char* filename) {
  File file = SD.open(filename, FILE_READ);
  if (!file) {
    Serial.println("❌ Failed to open file for reading.");
    return;
  }

  size_t totalSize = file.size();
  Serial.printf("📤 Preparing to upload %s (%d bytes)\n", filename, totalSize);
  
  // Start HTTPS session
  if (!sendAT("AT+CHTTPSSTART", 5000)) {
    Serial.println("❌ Failed to start HTTPS service.");
    file.close();
    return;
  }

  // Open session to server
  if (!sendAT("AT+CHTTPSOPSE=\"" + String(server) + "\"," + String(serverPort), 15000)) {
     Serial.println("❌ Failed to open HTTPS session to server.");
     sendAT("AT+CHTTPSSTOP", 5000);
     file.close();
     return;
  }

  bool uploadSuccess = true;
  for (size_t offset = 0; offset < totalSize; offset += CHUNK_SIZE) {
    size_t chunkSize = min((size_t)CHUNK_SIZE, totalSize - offset);
    
    // Build headers
    String headers = "Content-Type: application/octet-stream\r\n";
    headers += "X-Filename: " + String(filename) + "\r\n";
    headers += "X-Chunk-Offset: " + String(offset) + "\r\n";
    headers += "X-Chunk-Size: " + String(chunkSize) + "\r\n";
    headers += "X-Total-Size: " + String(totalSize) + "\r\n";
    
    // Prepare the POST request. This tells the modem how much header and content data to expect.
    String postCmd = "AT+CHTTPSPOST=\"/api/upload\"," + String(headers.length()) + "," + String(10000) + "," + String(chunkSize);
    
    Serial.println("[AT SEND] " + postCmd);
    modemSerial.println(postCmd);

    // Wait for the modem to be ready for the headers
    if (!expectResponse(">", 5000)) {
        Serial.println("❌ Modem did not respond to POST command. Aborting.");
        uploadSuccess = false;
        break;
    }
    
    // Send the headers
    Serial.println("[HEADERS] Sending " + String(headers.length()) + " bytes of headers.");
    modemSerial.print(headers);
    
    // The modem should be ready for the content data now.
    // The previous expectResponse should have consumed the first ">", but sometimes there's another.
    if (!expectResponse(">", 5000)) {
        Serial.println("❌ Modem not ready for content data. Aborting.");
        uploadSuccess = false;
        break;
    }

    // Send the content chunk
    Serial.println("[CONTENT] Sending " + String(chunkSize) + " bytes of content.");
    uint8_t buffer[chunkSize];
    file.read(buffer, chunkSize);
    modemSerial.write(buffer, chunkSize);
    modemSerial.flush();

    // Wait for the server's HTTP response (e.g., +CHTTPS: POST,200)
    if (!expectResponse("+CHTTPS: POST,200", 15000) && !expectResponse("+CHTTPS: POST,201", 1000)) {
        Serial.println("❌ Received non-200/201 response for chunk at offset " + String(offset));
        uploadSuccess = false;
        break; // Stop on first error
    }
    
    Serial.printf("✅ Chunk %d/%d uploaded successfully.\n", (offset/CHUNK_SIZE) + 1, (totalSize/CHUNK_SIZE) + 1);
    delay(500); // Small delay between chunks
  }

  // Close session and stop HTTPS
  sendAT("AT+CHTTPSCLSE", 5000);
  sendAT("AT+CHTTPSSTOP", 5000);
  file.close();

  if (uploadSuccess) {
    Serial.println("✅ File upload finished successfully!");
  } else {
    Serial.println("❌ File upload failed.");
  }
}
