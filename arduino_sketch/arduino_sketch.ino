// Define modem type and serial pins
#define TINY_GSM_MODEM_SIM7600
#define MODEM_RX 16
#define MODEM_TX 17
#define MODEM_BAUD 115200
#define SD_CS 5
#define CHUNK_SIZE 4096 // 4KB chunks
#define MAX_RETRIES 3
#define RETRY_DELAY_MS 2000

#include <HardwareSerial.h>
#include <SD.h>
#include <SPI.h>

// Use a unique name for the modem's serial port to avoid conflicts
HardwareSerial modemSerial(1);

// Server details
String server = "cellular-data-streamer.web.app";
int serverPort = 443;
String endpoint = "/api/upload";

/**
 * Sends an AT command and waits for a specific response.
 * @param command The AT command to send.
 * @param expectedResponse The response to wait for.
 * @param timeout How long to wait for the response.
 * @return True if the expected response was received, false otherwise.
 */
bool sendATCommand(const String& command, const String& expectedResponse, unsigned long timeout) {
  // Clear any lingering data from the serial buffer
  while (modemSerial.available()) {
    modemSerial.read();
  }

  Serial.println("[AT SEND] " + command);
  modemSerial.println(command);

  String response = "";
  unsigned long startTime = millis();
  while (millis() - startTime < timeout) {
    if (modemSerial.available()) {
      char c = modemSerial.read();
      response += c;
      if (response.indexOf(expectedResponse) != -1) {
        Serial.println("[AT RECV] " + response);
        return true;
      }
    }
  }
  Serial.println("[DEBUG] Timeout waiting for: " + expectedResponse);
  Serial.println("[DEBUG] Received: " + response);
  return false;
}


/**
 * Prints diagnostic information about the modem and network status.
 */
void debugModemStatus() {
  Serial.println("--- Modem Status ---");
  sendATCommand("ATI", "OK", 1000);
  sendATCommand("AT+CSQ", "OK", 1000);
  sendATCommand("AT+CPIN?", "OK", 1000);
  sendATCommand("AT+CCID", "OK", 1000);
  sendATCommand("AT+COPS?", "OK", 1000);
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
  modemSerial.begin(MODEM_BAUD, SERIAL_8N1, MODEM_RX, MODEM_TX);
  delay(3000);
  sendATCommand("AT+CRESET", "OK", 10000);
  delay(5000);
  Serial.println("✅ Modem restarted.");
  
  debugModemStatus();

  Serial.println("📡 Connecting to network...");
  sendATCommand("AT+CGATT=1", "OK", 10000);
  if (!sendATCommand("AT+CNACT=1,\"vodafone\"", "ACTIVE", 20000)) {
    Serial.println("❌ Failed to activate network context.");
    while(true);
  }
  Serial.println("✅ GPRS connected.");
  sendATCommand("AT+IPADDR", "OK", 1000);


  // --- NEW SSL CONFIGURATION ---
  Serial.println("🔧 Configuring SSL/TLS...");
  // Use SSL context 1
  // Set SSL version to allow all (TLS 1.0 -> 1.3) for broad compatibility
  sendATCommand("AT+CSSLCFG=\"sslversion\",1,4", "OK", 2000);
  // Set authentication mode to 0 (no server cert verification)
  sendATCommand("AT+CSSLCFG=\"authmode\",1,0", "OK", 2000);
  // Configure HTTPS to use our SSL context (index 1)
  sendATCommand("AT+CHTTPSCFG=1,1", "OK", 2000);
  Serial.println("✅ SSL Configured.");
  // --- END OF NEW SSL CONFIGURATION ---
}

void loop() {
  Serial.println("📤 Preparing to upload /sigma2.wav");
  sendFileChunks("/sigma2.wav");
  delay(30000); // Wait 30 seconds before trying again
}

/**
 * Uploads a file from the SD card in chunks using raw AT commands.
 * @param filename The name of the file to upload.
 */
void sendFileChunks(const char* filename) {
  File file = SD.open(filename);
  if (!file) {
    Serial.println("❌ Failed to open file.");
    return;
  }

  size_t totalSize = file.size();
  Serial.printf("📤 Preparing to upload %s (%d bytes)\n", filename, totalSize);

  // Start HTTPS session
  if (!sendATCommand("AT+CHTTPSSTART", "OK", 5000)) {
     Serial.println("❌ Failed to start HTTPS service.");
     file.close();
     return;
  }

  // Open session with server
  if (!sendATCommand("AT+CHTTPSOPSE=\"" + server + "\"," + serverPort, "+CHTTPSOPSE: 0", 20000)) {
      Serial.println("❌ Failed to open HTTPS session with server.");
      sendATCommand("AT+CHTTPSSTOP", "OK", 5000); // Stop service on failure
      file.close();
      return;
  }

  size_t offset = 0;
  while (offset < totalSize) {
    bool chunkSuccess = false;
    for (int retry = 0; retry < MAX_RETRIES; retry++) {
      Serial.printf("  Attempt %d/%d to send chunk at offset %d...\n", retry + 1, MAX_RETRIES, offset);
      
      file.seek(offset);
      uint8_t buffer[CHUNK_SIZE];
      size_t chunkSize = file.read(buffer, CHUNK_SIZE);
      
      if (chunkSize == 0) {
        Serial.println("  Read 0 bytes, assuming end of file.");
        chunkSuccess = true;
        break;
      }
      
      String headers = "X-Filename: " + String(filename) + "\r\n" +
                       "X-Chunk-Offset: " + String(offset) + "\r\n" +
                       "X-Chunk-Size: " + String(chunkSize) + "\r\n" +
                       "X-Total-Size: " + String(totalSize) + "\r\n";
      
      // Corrected AT+CHTTPSPOST: <path>,<header_len>,<content_len>,<content_timeout>
      String postCmd = "AT+CHTTPSPOST=\"" + endpoint + "\"," + headers.length() + "," + chunkSize + ",10000";

      if (sendATCommand(postCmd, ">", 5000)) {
        // Modem is ready for data, send headers then chunk
        Serial.println("[AT SEND >] " + headers);
        modemSerial.print(headers);
        modemSerial.write(buffer, chunkSize);
        
        // Wait for the server response from the modem
        String response = "";
        unsigned long startTime = millis();
        bool gotResponse = false;
        while(millis() - startTime < 20000) { // 20-second timeout for server response
          if(modemSerial.available()){
            char c = modemSerial.read();
            response += c;
            // Check for a 2xx success code
            if (response.indexOf("+CHTTPS: POST,20") != -1) {
              gotResponse = true;
              chunkSuccess = true;
              break;
            } else if (response.indexOf("ERROR") != -1) {
              gotResponse = true;
              chunkSuccess = false;
              break;
            }
          }
        }
        
        Serial.println("[AT RECV] " + response);
        if (chunkSuccess) {
          Serial.printf("  ✅ Chunk at offset %d sent successfully.\n", offset);
          break; // Exit retry loop on success
        } else {
          Serial.println("  ❌ Chunk upload failed. Server did not return 2xx.");
        }
      } else {
        Serial.println("  ❌ Modem did not respond to POST command.");
      }
      delay(RETRY_DELAY_MS); // Wait before retrying
    }

    if (!chunkSuccess) {
      Serial.printf("❌ Failed to upload chunk at offset %d after %d retries. Aborting.\n", offset, MAX_RETRIES);
      break; // Exit main while loop
    }

    offset += chunkSize;
  }

  // Close server session and stop HTTPS service
  sendATCommand("AT+CHTTPSCLSE", "OK", 5000);
  sendATCommand("AT+CHTTPSSTOP", "OK", 5000);

  file.close();

  if (offset >= totalSize) {
    Serial.println("✅ File upload finished.");
  } else {
    Serial.println("❌ File upload failed.");
  }
}
