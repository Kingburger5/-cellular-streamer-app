
#define TINY_GSM_MODEM_SIM7600
#include <HardwareSerial.h>
#include <SD.h>
#include <SPI.h>

// ===== SERIAL PORTS & PINS =====
#define MODEM_TX 17
#define MODEM_RX 16
#define MODEM_BAUD 115200
#define SD_CS 5

// ===== FILE UPLOAD SETTINGS =====
#define CHUNK_SIZE 4096 // Size of each chunk in bytes
#define MAX_RETRIES 3   // Number of retries for each chunk

// ===== SERVER DETAILS =====
// IMPORTANT: Update this to your development server URL for testing.
// This should be the host only, without "http://" or a port number.
const char* server = "cellular-data-streamer.web.app";
const int serverPort = 443;
const char* endpoint = "/api/upload";

// ===== GLOBALS =====
HardwareSerial modemSerial(1); // Use UART 1 for the modem

// ===== FORWARD DECLARATIONS =====
void sendFileChunks(const char* filename);
bool sendATCommand(const String& cmd, const String& expectedResponse, String* response, unsigned long timeout);
bool configureSSL();
bool openHttpsSession();
void closeHttpsSession();

// =================================================================
//                             SETUP
// =================================================================
void setup() {
  Serial.begin(115200);
  while (!Serial);
  delay(1000);

  Serial.println("\n🔌 Booting...");

  // Initialize SD Card
  if (!SD.begin(SD_CS)) {
    Serial.println("❌ SD card failed or not present.");
    while (true);
  }
  Serial.println("✅ SD card ready.");

  // Initialize Modem
  Serial.println("Initializing modem...");
  modemSerial.begin(MODEM_BAUD, SERIAL_8N1, MODEM_RX, MODEM_TX);
  delay(3000);

  // Send a basic AT command to check if modem is responsive
  if (!sendATCommand("AT", "OK", NULL, 1000)) {
     Serial.println("❌ Modem not responding. Check connections and power.");
     while(true);
  }
  
  Serial.println("✅ Modem restarted.");
  Serial.println("--- Modem Status ---");
  sendATCommand("ATI", "OK", NULL, 1000);
  sendATCommand("AT+CSQ", "OK", NULL, 1000);
  sendATCommand("AT+CPIN?", "OK", NULL, 1000);
  sendATCommand("AT+CCID", "OK", NULL, 1000);
  sendATCommand("AT+COPS?", "OK", NULL, 1000);
  Serial.println("--------------------");

  // Configure SSL/TLS settings
  if (!configureSSL()) {
    Serial.println("❌ Failed to configure SSL context.");
    while (true);
  }

  // Connect to network
  Serial.println("📡 Connecting to network...");
  if (!sendATCommand("AT+CGATT=1", "OK", NULL, 10000)) {
    Serial.println("❌ Failed to attach to GPRS.");
    while(true);
  }
  Serial.println("✅ GPRS connected.");

  // Get local IP address
  sendATCommand("AT+IPADDR", "+IPADDR", NULL, 5000);

  // Start the upload process
  sendFileChunks("/sigma2.wav");
}

// =================================================================
//                              LOOP
// =================================================================
void loop() {
  // Keep empty. The process runs once in setup().
}


// =================================================================
//                        CORE FUNCTIONS
// =================================================================

/**
 * @brief Configures the SSL context for HTTPS connections.
 * This function sets the SSL version and tells the modem not to verify the server certificate,
 * which is often necessary for development environments or self-signed certificates.
 * @return true if SSL was configured successfully, false otherwise.
 */
bool configureSSL() {
    Serial.println("🔧 Configuring SSL...");
    // Set SSL context to not validate server certificate (level 0)
    if (!sendATCommand("AT+CSSLCFG=\"sslversion\",1,3", "OK", NULL, 2000)) return false;
    if (!sendATCommand("AT+CSSLCFG=\"authmode\",1,0", "OK", NULL, 2000)) return false;
    if (!sendATCommand("AT+SHSSL=1,1", "OK", NULL, 2000)) return false;
    Serial.println("✅ SSL context configured.");
    return true;
}

/**
 * @brief Opens a new HTTPS session with the server.
 * @return true if the session was opened successfully, false otherwise.
 */
bool openHttpsSession() {
  if (!sendATCommand("AT+CHTTPSSTART", "OK", NULL, 5000)) return false;
  String opseCmd = "AT+CHTTPSOPSE=\"" + String(server) + "\"," + String(serverPort);
  // Wait specifically for "+CHTTPSOPSE: 0" which indicates success
  if (!sendATCommand(opseCmd, "+CHTTPSOPSE: 0", NULL, 15000)) {
     Serial.println("❌ Failed to open HTTPS session with server.");
     return false;
  }
  return true;
}

/**
 * @brief Closes the current HTTPS session.
 */
void closeHttpsSession() {
  sendATCommand("AT+CHTTPSCLSE", "OK", NULL, 5000);
  sendATCommand("AT+CHTTPSSTOP", "OK", NULL, 5000);
}


/**
 * @brief Uploads a file from the SD card to the server in chunks.
 * Uses raw AT commands for HTTPS POST requests.
 * @param filename The full path of the file to upload (e.g., "/data.txt").
 */
void sendFileChunks(const char* filename) {
  File file = SD.open(filename);
  if (!file) {
    Serial.printf("❌ Failed to open file: %s\n", filename);
    return;
  }

  size_t totalSize = file.size();
  Serial.printf("📤 Preparing to upload %s (%d bytes)\n", filename, totalSize);
  
  size_t offset = 0;
  bool uploadOk = true;

  while (offset < totalSize) {
    // --- 1. Prepare Chunk Data ---
    size_t chunkSize = min((size_t)CHUNK_SIZE, totalSize - offset);
    uint8_t buffer[chunkSize];
    file.seek(offset);
    file.read(buffer, chunkSize);

    bool chunkSent = false;
    for (int retry = 0; retry < MAX_RETRIES; retry++) {
      Serial.printf("  Attempt %d/%d to send chunk at offset %d...\n", retry + 1, MAX_RETRIES, offset);

      if (openHttpsSession()) {
        // --- 2. Construct Headers ---
        String headers = "X-Filename: " + String(filename) + "\r\n";
        headers += "X-Chunk-Offset: " + String(offset) + "\r\n";
        headers += "X-Chunk-Size: " + String(chunkSize) + "\r\n";
        headers += "X-Total-Size: " + String(totalSize) + "\r\n";

        // --- 3. Send POST Command ---
        long timeout = 20000; // 20-second timeout for the whole POST operation
        String postCmd = "AT+CHTTPSPOST=\"" + String(endpoint) + "\"," + String(headers.length()) + "," + String(chunkSize) + "," + String(timeout);
        
        String response;
        if (sendATCommand(postCmd, ">", &response, 5000)) {
          // --- 4. Send Headers and Payload ---
          modemSerial.print(headers);
          modemSerial.write(buffer, chunkSize);
          modemSerial.flush();

          // --- 5. Check for Success Response ---
          if (sendATCommand("", "+CHTTPS: POST,200", &response, timeout)) {
             Serial.printf("✅ Chunk sent successfully (Offset: %d, Size: %d)\n", offset, chunkSize);
             chunkSent = true;
          } else {
             Serial.println("❌ Chunk upload failed: Server did not respond with 200 OK.");
             Serial.println("[SERVER RESPONSE]");
             Serial.println(response);
          }
        } else {
           Serial.println("❌ Modem did not respond to POST command. Aborting.");
        }
        
        // --- 6. Clean Up Session ---
        closeHttpsSession();

      } else {
        Serial.println("❌ Could not establish HTTPS session for chunk.");
      }

      if (chunkSent) {
        break; // Exit retry loop if successful
      }
      
      delay(3000); // Wait before retrying
    }

    if (!chunkSent) {
      Serial.printf("❌ Failed to upload chunk at offset %d after %d retries. Aborting.\n", offset, MAX_RETRIES);
      uploadOk = false;
      break; // Exit main while loop
    }

    offset += chunkSize;
    delay(500); // Small delay between chunks
  }

  file.close();
  if (uploadOk) {
    Serial.println("✅ File upload finished successfully.");
  } else {
    Serial.println("❌ File upload failed.");
  }
}


/**
 * @brief Sends an AT command and waits for an expected response.
 * @param cmd The AT command to send.
 * @param expectedResponse The string to look for in the response.
 * @param response Pointer to a String to store the full response.
 * @param timeout The maximum time to wait for the response.
 * @return true if the expected response was found, false otherwise.
 */
bool sendATCommand(const String& cmd, const String& expectedResponse, String* response, unsigned long timeout) {
  if (cmd.length() > 0) {
    Serial.println("[AT SEND] " + cmd);
    modemSerial.println(cmd);
  }

  unsigned long start = millis();
  String buffer = "";
  
  while (millis() - start < timeout) {
    if (modemSerial.available()) {
      char c = modemSerial.read();
      buffer += c;
    }
    if (buffer.indexOf(expectedResponse) != -1) {
      if (response) *response = buffer;
      // Serial.println("[AT RECV] " + buffer); // Verbose logging
      return true;
    }
  }

  Serial.println("[DEBUG] Timeout waiting for: " + expectedResponse);
  Serial.println("[DEBUG] Received: " + buffer);
  if (response) *response = buffer;
  return false;
}
