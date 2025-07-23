// Define modem type
#define TINY_GSM_MODEM_SIM7600

#include <HardwareSerial.h>
#include <SD.h>
#include <SPI.h>

// Modem pins
#define MODEM_TX 17
#define MODEM_RX 16
#define MODEM_BAUD 115200

// SD card chip select
#define SD_CS 5

// File upload settings
#define CHUNK_SIZE 4096 // Size of each chunk in bytes
#define MAX_RETRIES 3   // Number of retries for a failed chunk
#define RETRY_DELAY 2000 // Delay between retries in milliseconds
#define RESPONSE_TIMEOUT 10000 // Timeout for waiting for server response

// Server details
const char* server = "cellular-data-streamer.web.app";
const int serverPort = 443;
const char* endpoint = "/api/upload";

// Serial interfaces
HardwareSerial modemSerial(1); // For modem communication
#define SerialMon Serial       // For serial monitor output

// --- Forward Declarations ---
bool sendFileChunks(const char* filename);
bool sendChunk(const uint8_t* buffer, size_t chunkSize, size_t offset, size_t totalSize, const char* filename);
bool readResponseUntil(const char* target, unsigned long timeout);

void setup() {
  SerialMon.begin(115200);
  delay(1000);
  SerialMon.println("? Booting...");

  // Start SD card
  if (!SD.begin(SD_CS)) {
    SerialMon.println("? SD card failed to initialize or is not present.");
    while (true);
  }
  SerialMon.println("? SD card ready.");

  // Start modem serial
  modemSerial.begin(MODEM_BAUD, SERIAL_8N1, MODEM_RX, MODEM_TX);
  delay(3000);

  SerialMon.println("Initializing modem...");
  modemSerial.println("AT");
  delay(100);
  modemSerial.println("ATE0"); // Disable echo
  delay(100);
  modemSerial.println("AT+CMEE=2"); // Enable verbose errors
  delay(100);
  SerialMon.println("? Modem restarted.");
  
  // Debug Modem Status
  SerialMon.println("--- Modem Status ---");
  modemSerial.println("ATI");
  delay(500);
  modemSerial.println("AT+CSQ");
  delay(500);
  modemSerial.println("AT+CPIN?");
  delay(500);
  modemSerial.println("AT+CCID");
  delay(500);
  modemSerial.println("AT+COPS?");
  delay(1000);
  while(modemSerial.available()) { SerialMon.write(modemSerial.read()); }
  SerialMon.println("--------------------");

  SerialMon.println("? Connecting to network...");
  modemSerial.println("AT+CGDCONT=1,\"IP\",\"\""); // APN for vodafone is blank
  delay(1000);
  modemSerial.println("AT+CGACT=1,1");
  delay(5000);

  // Check GPRS connection
  modemSerial.println("AT+CGATT?");
  readResponseUntil("OK", 2000);
  
  SerialMon.println("? GPRS connected.");
  modemSerial.println("AT+IPADDR");
  readResponseUntil("OK", 2000);

  // Upload the file
  sendFileChunks("/sigma2.wav");
}

void loop() {
  // Keep the main loop clean.
  delay(10000);
}

bool sendFileChunks(const char* filename) {
  File file = SD.open(filename);
  if (!file) {
    SerialMon.printf("? Failed to open file: %s\n", filename);
    return false;
  }

  size_t totalSize = file.size();
  SerialMon.printf("? Preparing to upload %s (%d bytes)\n", filename, totalSize);

  // Start HTTPS session
  modemSerial.println("AT+CHTTPSSTART");
  if (!readResponseUntil("OK", 5000)) {
    SerialMon.println("? Failed to start HTTPS service.");
    return false;
  }
  
  // Open HTTPS connection to the server
  modemSerial.printf("AT+CHTTPSOPSE=\"%s\",%d\n", server, serverPort);
  if (!readResponseUntil("+CHTTPSOPSE: 0", 15000)) {
     SerialMon.println("? Failed to open HTTPS session with server.");
     modemSerial.println("AT+CHTTPSSTOP");
     readResponseUntil("OK", 2000);
     return false;
  }

  size_t offset = 0;
  bool success = true;
  while (offset < totalSize) {
    size_t chunkSize = min((size_t)CHUNK_SIZE, totalSize - offset);
    uint8_t buffer[chunkSize];
    
    file.seek(offset);
    file.read(buffer, chunkSize);

    if (!sendChunk(buffer, chunkSize, offset, totalSize, filename)) {
      SerialMon.printf("? Failed to upload chunk at offset %d after %d retries. Aborting.\n", offset, MAX_RETRIES);
      success = false;
      break;
    }
    offset += chunkSize;
  }
  
  file.close();

  // Close HTTPS session
  modemSerial.println("AT+CHTTPSCLSE");
  readResponseUntil("OK", 5000);
  modemSerial.println("AT+CHTTPSSTOP");
  readResponseUntil("OK", 5000);

  if (success) {
    SerialMon.println("? File upload completed successfully.");
  } else {
    SerialMon.println("? File upload failed.");
  }

  return success;
}


bool sendChunk(const uint8_t* buffer, size_t chunkSize, size_t offset, size_t totalSize, const char* filename) {
  for (int retry = 0; retry < MAX_RETRIES; retry++) {
    SerialMon.printf("  Attempt %d/%d to send chunk at offset %d...\n", retry + 1, MAX_RETRIES, offset);
    
    // Construct headers
    String headers = "Content-Type: application/octet-stream\r\n";
    headers += "X-Filename: " + String(filename) + "\r\n";
    headers += "X-Chunk-Offset: " + String(offset) + "\r\n";
    headers += "X-Chunk-Size: " + String(chunkSize) + "\r\n";
    headers += "X-Total-Size: " + String(totalSize) + "\r\n";

    // Prepare the POST command
    // Syntax: AT+CHTTPSPOST=<path>,<header_len>,<content_len>,<timeout_ms>
    String postCommand = String("AT+CHTTPSPOST=\"") + endpoint + "\"," + headers.length() + "," + chunkSize + "," + RESPONSE_TIMEOUT;
    modemSerial.println(postCommand);
    
    // Wait for the ">" prompt
    if (readResponseUntil(">", 5000)) {
      // Send headers
      modemSerial.print(headers);
      // Send binary data
      modemSerial.write(buffer, chunkSize);
      
      // Wait for the server response (e.g., "+CHTTPS: POST,200" or "+CHTTPS: POST,201")
      if (readResponseUntil("+CHTTPS: POST,20", 20000)) { // Check for 200, 201
          SerialMon.println("  ? Chunk uploaded successfully.");
          return true;
      } else {
          SerialMon.println("  ? Server returned an error or timed out.");
      }
    } else {
      SerialMon.println("? Modem did not respond to POST command. Aborting.");
    }

    SerialMon.printf("  ? Chunk failed. Retrying in %dms...\n", RETRY_DELAY);
    delay(RETRY_DELAY);
  }
  return false;
}

bool readResponseUntil(const char* target, unsigned long timeout) {
  unsigned long start = millis();
  String response = "";
  char c;

  SerialMon.print("[AT RECV] ");
  while (millis() - start < timeout) {
    if (modemSerial.available()) {
      c = modemSerial.read();
      SerialMon.print(c);
      response += c;
      if (response.indexOf(target) != -1) {
        // Read any remaining data on the line
        while(modemSerial.available()) {
            SerialMon.print((char)modemSerial.read());
        }
        return true;
      }
    }
  }
  SerialMon.println("\n[DEBUG] Timeout waiting for: " + String(target));
  SerialMon.println("[DEBUG] Received: " + response);
  return false;
}
