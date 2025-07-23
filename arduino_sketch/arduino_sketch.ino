// THIS IS THE CORRECT FILE FOR THE ARDUINO SKETCH
#include <SPI.h>
#include <SD.h>

#define TINY_GSM_MODEM_SIM7600
#define SerialMon Serial
#define TINY_GSM_DEBUG SerialMon
#include <TinyGsmClient.h>

// Pin definitions
#define MODEM_RX 16 // Board RX, connects to TX of the modem
#define MODEM_TX 17 // Board TX, connects to RX of the modem
#define MODEM_BAUD 115200
#define SD_CS_PIN 5

// APN credentials (replace with your provider's details)
const char apn[] = "hologram"; // APN
const char gprsUser[] = "";    // GPRS User
const char gprsPass[] = "";    // GPRS Password

// Server details
const char server[] = "cellular-data-streamer.web.app";
const char resource[] = "/api/upload";
const int serverPort = 443;

// File upload settings
const int MAX_RETRIES = 3;
const unsigned long CHUNK_UPLOAD_TIMEOUT = 20000L; // 20 seconds
const int CHUNK_BUFFER_SIZE = 4096;

// Modem and serial setup
HardwareSerial modemSerial(1);
TinyGsm modem(modemSerial);

// --- HELPER FUNCTIONS ---

// Function to send an AT command and wait for a specific response
bool sendAT(const String& command, const char* expected_response, unsigned long timeout = 10000L) {
  unsigned long startTime = millis();
  String response = "";

  SerialMon.print("[AT SEND] ");
  SerialMon.println(command);

  modemSerial.println(command);

  do {
    while (modemSerial.available()) {
      char c = modemSerial.read();
      response += c;
      SerialMon.print(c); // Print live response for debugging
    }
  } while (millis() - startTime < timeout && response.indexOf(expected_response) == -1);

  if (response.indexOf(expected_response) != -1) {
    return true;
  } else {
    SerialMon.println();
    SerialMon.println("[DEBUG] Timeout waiting for: " + String(expected_response));
    SerialMon.println("[DEBUG] Received: " + response);
    return false;
  }
}

// Function to read the entire response until a timeout
String readResponse(unsigned long timeout = 1000) {
    String response = "";
    unsigned long startTime = millis();
    while ((millis() - startTime) < timeout) {
        if (modemSerial.available()) {
            response += (char)modemSerial.read();
        }
    }
    return response;
}


// --- CORE FUNCTIONS ---

void restartModem() {
  SerialMon.println("Initializing modem...");
  modem.restart();
  if (sendAT("AT", "OK", 5000)) {
     SerialMon.println("✅ Modem restarted.");
  } else {
     SerialMon.println("❌ Modem not responding.");
  }
}

void printModemStatus() {
  SerialMon.println("--- Modem Status ---");
  sendAT("ATI", "OK", 1000);
  sendAT("AT+CSQ", "OK", 1000);
  sendAT("AT+CPIN?", "OK", 1000);
  sendAT("AT+CCID", "OK", 1000);
  sendAT("AT+COPS?", "OK", 1000);
  SerialMon.println("--------------------");
}

bool configureSSL() {
    SerialMon.println("🔧 Configuring SSL...");
    if (!sendAT("AT+CSSLCFG=\"sslversion\",1,3", "OK")) return false; // Allow TLS 1.2
    if (!sendAT("AT+CSSLCFG=\"authmode\",1,0", "OK")) return false;   // No server cert validation
    SerialMon.println("✅ SSL Configured.");
    return true;
}

bool openHttpsSession() {
  if (!sendAT("AT+CHTTPSSTART", "OK")) return false;
  
  // Wait for the +CHTTPSSTART: 0 confirmation.
  if(!sendAT("", "+CHTTPSSTART: 0", 5000)) {
    SerialMon.println("❌ Modem did not confirm HTTPS start.");
    return false;
  }
  
  String cmd = "AT+CHTTPSOPSE=\"" + String(server) + "\"," + String(serverPort);
  if (!sendAT(cmd, "OK")) return false;

  // Wait for the +CHTTPSOPSE: 0 confirmation.
  if (!sendAT("", "+CHTTPSOPSE: 0", 20000L)) {
      SerialMon.println("❌ Failed to open HTTPS session with server.");
      return false;
  }

  SerialMon.println("✅ HTTPS Session Opened.");
  return true;
}

void closeHttpsSession() {
    sendAT("AT+CHTTPSCLSE", "OK", 5000);
    sendAT("AT+CHTTPSSTOP", "OK", 5000);
    SerialMon.println("✅ HTTPS Session Closed.");
}

bool sendChunk(File& file, const char* filename, size_t fileSize, size_t offset) {
    uint8_t chunkBuffer[CHUNK_BUFFER_SIZE] = {0};
    size_t chunkSize = file.read(chunkBuffer, CHUNK_BUFFER_SIZE);

    if (chunkSize == 0) return true; // No more data to read

    // Construct headers
    String headers = "x-filename: " + String(filename) + "\r\n" +
                   "x-chunk-offset: " + String(offset) + "\r\n" +
                   "x-chunk-size: " + String(chunkSize) + "\r\n" +
                   "x-total-size: " + String(fileSize) + "\r\n" +
                   "Content-Type: application/octet-stream";

    // Prepare POST command: path, header length, content length, timeout
    String cmd = "AT+CHTTPSPOST=\"" + String(resource) + "\"," +
                 String(headers.length()) + "," + String(chunkSize) + ",20000";

    if (!sendAT(cmd, ">")) {
        SerialMon.println("❌ Modem did not respond to POST command. Aborting.");
        return false;
    }

    // Send headers
    modemSerial.println(headers);

    // Send binary chunk data
    modemSerial.write(chunkBuffer, chunkSize);
    modemSerial.flush();

    // Wait for response, e.g., "+CHTTPS: POST,200,"
    if (!sendAT("", "+CHTTPS: POST,200", CHUNK_UPLOAD_TIMEOUT)) {
        SerialMon.println("❌ Chunk upload failed: Did not get 200 OK.");
        return false;
    }

    Serial.printf("✅ Chunk %d/%d uploaded successfully.\n", (offset / CHUNK_BUFFER_SIZE) + 1, (fileSize / CHUNK_BUFFER_SIZE) + 1);
    return true;
}


void sendFileInChunks(const char* filename) {
  File file = SD.open(filename, FILE_READ);
  if (!file) {
    SerialMon.printf("❌ Failed to open %s\n", filename);
    return;
  }

  size_t fileSize = file.size();
  SerialMon.printf("📤 Preparing to upload %s (%d bytes)\n", filename, fileSize);
  
  size_t offset = 0;
  bool uploadOk = true;

  if (openHttpsSession()) {
      while (offset < fileSize) {
          bool chunkSuccess = false;
          for (int retry = 0; retry < MAX_RETRIES; retry++) {
              if (sendChunk(file, filename, fileSize, offset)) {
                  chunkSuccess = true;
                  break;
              }
              Serial.printf("  Retrying chunk at offset %d...\n", offset);
          }

          if (chunkSuccess) {
              offset += CHUNK_BUFFER_SIZE;
          } else {
              Serial.printf("❌ Failed to upload chunk at offset %d after %d retries. Aborting.\n", offset, MAX_RETRIES);
              uploadOk = false;
              break;
          }
      }
      closeHttpsSession();
  } else {
      uploadOk = false;
  }

  if (uploadOk) {
    SerialMon.println("✅ File upload completed successfully!");
  } else {
    SerialMon.println("❌ File upload failed.");
  }
  file.close();
}


void setup() {
  SerialMon.begin(115200);
  delay(10);
  SerialMon.println("\n🔌 Booting...");

  if (!SD.begin(SD_CS_PIN)) {
    SerialMon.println("❌ SD card initialization failed!");
    while (1);
  }
  SerialMon.println("✅ SD card ready.");
  
  modemSerial.begin(MODEM_BAUD, SERIAL_8N1, MODEM_RX, MODEM_TX);
  
  restartModem();
  printModemStatus();

  if (!configureSSL()) {
      SerialMon.println("❌ Failed to configure SSL context.");
      // We can decide to halt or proceed, for now let's try
  }

  SerialMon.println("📡 Connecting to network...");
  if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
    SerialMon.println("❌ GPRS connection failed.");
    return;
  }
  SerialMon.println("✅ GPRS connected.");
  sendAT("AT+IPADDR", "OK");

  sendFileInChunks("/sigma2.wav");
}

void loop() {
  // Nothing to do here, all logic is in setup()
  delay(10000);
}
