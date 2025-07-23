#include <Arduino.h>
#include "FS.h"
#include "SD.h"

// Use a separate serial for the modem
#define modemSerial Serial1

// Define the serial pins for the modem
#define MODEM_RX 27
#define MODEM_TX 26
#define MODEM_PWRKEY 4
#define MODEM_DTR 32
#define MODEM_RI 33
#define MODEM_FLIGHT 25
#define MODEM_STATUS 34

// Baud rate for the modem
#define MODEM_BAUD 115200

// Your GPRS credentials
const char apn[] = "internet"; // APN
const char gprsUser[] = "";    // GPRS User
const char gprsPass[] = "";    // GPRS Password

// Server details
const char server[] = "cellular-data-streamer.web.app";
const int serverPort = 443; // HTTPS port

// Pin for the SD card CS
#define SD_CS 5

// --- Function Prototypes ---
void modemPowerOn();
bool setupModem();
bool setupGPRS();
bool sendChunk(fs::File &file, const char* filename, size_t offset, size_t totalSize);
void sendFileChunks(const char *filename);
void printModemStatus();
bool sendATCommand(const char* cmd, const char* expect, unsigned long timeout, bool debug = false, char* out_response = nullptr, size_t out_len = 0);
bool configureSSL();
bool openHttpsSession();
void closeHttpsSession();


// --- Setup ---
void setup() {
  Serial.begin(115200);
  while (!Serial);

  Serial.println("? Booting...");

  // Initialize SD card
  if (!SD.begin(SD_CS)) {
    Serial.println("? SD card initialization failed!");
    return;
  }
  Serial.println("? SD card ready.");
  
  modemPowerOn();
  
  modemSerial.begin(MODEM_BAUD, SERIAL_8N1, MODEM_RX, MODEM_TX);

  if (!setupModem()) {
    return;
  }

  if(!configureSSL()){
    Serial.println("? Failed to configure SSL context.");
    return;
  }
  
  if (!setupGPRS()) {
    return;
  }

  printModemStatus();

  // The file to upload
  const char *filename = "/sigma2.wav";
  sendFileChunks(filename);
}

// --- Loop ---
void loop() {
  // Keep the main loop clean, all logic is in setup for this example
}

// --- Modem and Network Functions ---

void modemPowerOn() {
  pinMode(MODEM_PWRKEY, OUTPUT);
  digitalWrite(MODEM_PWRKEY, LOW);
  delay(1000);
  digitalWrite(MODEM_PWRKEY, HIGH);
  delay(2000);
  digitalWrite(MODEM_PWRKEY, LOW);
}

bool setupModem() {
  Serial.println("Initializing modem...");
  if (!sendATCommand("AT", "OK", 3000)) {
     Serial.println("? Modem not responding.");
     return false;
  }
  sendATCommand("AT+CREG?", "OK", 1000);
  sendATCommand("ATE0", "OK", 1000); // Disable echo
  Serial.println("? Modem restarted.");
  return true;
}

bool configureSSL() {
    Serial.println("? Configuring SSL...");
    if (!sendATCommand("AT+CSSLCFG=\"sslversion\",1,3", "OK", 5000)) return false; // Use TLS 1.2
    if (!sendATCommand("AT+CSSLCFG=\"authmode\",1,0", "OK", 5000)) return false; // Don't require server cert validation
    Serial.println("? SSL configured.");
    return true;
}


bool setupGPRS() {
  Serial.println("? Connecting to network...");
  if (!sendATCommand("AT+CGATT=1", "OK", 10000)) {
    Serial.println("? Failed to attach to GPRS.");
    return false;
  }
  
  String cmd = "AT+CSTT=\"" + String(apn) + "\",\"" + String(gprsUser) + "\",\"" + String(gprsPass) + "\"";
  if (!sendATCommand(cmd.c_str(), "OK", 10000)) return false;
  
  if (!sendATCommand("AT+CIICR", "OK", 20000)) {
    Serial.println("? Failed to bring up wireless connection.");
    return false;
  }
  Serial.println("? GPRS connected.");
  return true;
}

bool openHttpsSession() {
  if (!sendATCommand("AT+CHTTPSSTART", "OK", 10000)) {
     Serial.println("? Failed to start HTTPS service.");
     return false;
  }

  char cmd[200];
  sprintf(cmd, "AT+CHTTPSOPSE=\"%s\",%d", server, serverPort);
  if (!sendATCommand(cmd, "+CHTTPSOPSE: 0", 20000)) { // Wait for the specific success code
    Serial.println("? Failed to open HTTPS session with server.");
    closeHttpsSession();
    return false;
  }

  return true;
}

void closeHttpsSession() {
    sendATCommand("AT+CHTTPSCLSE", "OK", 5000);
    sendATCommand("AT+CHTTPSSTOP", "OK", 5000);
}


bool sendChunk(fs::File &file, const char* filename, size_t offset, size_t totalSize) {
    size_t chunkSize = file.size() - offset;
    if (chunkSize > 4096) {
        chunkSize = 4096;
    }
    
    // Correct buffer type: uint8_t[]
    uint8_t chunkBuffer[chunkSize];
    size_t bytesRead = file.read(chunkBuffer, chunkSize);
    if(bytesRead != chunkSize) {
        Serial.println("? File read error.");
        return false;
    }

    char headers[200];
    sprintf(headers,
        "X-Filename: %s\r\n"
        "X-Chunk-Offset: %zu\r\n"
        "X-Chunk-Size: %zu\r\n"
        "X-Total-Size: %zu\r\n",
        filename, offset, chunkSize, totalSize);
    
    size_t headerLen = strlen(headers);
    size_t totalPayloadSize = headerLen + chunkSize;

    char cmd[200];
    sprintf(cmd, "AT+CHTTPSPOST=\"/api/upload\",%zu,%zu", totalPayloadSize, 20000);

    if (!sendATCommand(cmd, ">", 10000)) {
        Serial.println("? Modem did not respond to POST command. Aborting.");
        return false;
    }

    // Send headers then chunk data
    modemSerial.write(headers, headerLen);
    modemSerial.write(chunkBuffer, chunkSize);

    if (!sendATCommand(nullptr, "+CHTTPS: POST,200", 30000)) { // Wait for 200 OK
        Serial.println("? Server did not return 200 OK.");
        return false;
    }

    Serial.printf("? Chunk at offset %zu sent successfully.\n", offset);
    return true;
}

void sendFileChunks(const char *filename) {
  File file = SD.open(filename, FILE_READ);
  if (!file) {
    Serial.println("? Failed to open file for reading.");
    return;
  }

  size_t fileSize = file.size();
  Serial.printf("? Preparing to upload %s (%zu bytes)\n", filename, fileSize);

  size_t offset = 0;
  bool uploadFailed = false;

  while (offset < fileSize) {
    bool success = false;
    
    // Open session for each chunk to be safe
    if (!openHttpsSession()) {
        uploadFailed = true;
        break;
    }

    for (int retry = 0; retry < 3; retry++) {
        Serial.printf("  Attempt %d/3 to send chunk at offset %zu...\n", retry + 1, offset);
        
        // Seek to the correct position in the file for this chunk
        file.seek(offset);

        if (sendChunk(file, filename, offset, fileSize)) {
            success = true;
            break;
        } else {
             Serial.printf("    ...upload of chunk at offset %zu failed.\n", offset);
             delay(2000); // Wait before retrying
        }
    }
    
    closeHttpsSession(); // Close session after trying a chunk

    if (!success) {
      Serial.printf("? Failed to upload chunk at offset %zu after 3 retries. Aborting.\n", offset);
      uploadFailed = true;
      break;
    }
    
    // This logic is tricky. Re-reading chunksize is required.
    file.seek(offset);
    size_t chunkSize = file.size() - offset;
    if (chunkSize > 4096) {
        chunkSize = 4096;
    }
    uint8_t temp_buffer[chunkSize];
    size_t bytesRead = file.read(temp_buffer, chunkSize);
    offset += bytesRead;
  }

  if (uploadFailed) {
    Serial.println("? File upload failed.");
  } else {
    Serial.println("? File upload successful!");
  }

  file.close();
}


void printModemStatus() {
  char response[100];
  Serial.println("--- Modem Status ---");
  sendATCommand("ATI", "OK", 1000, false, response, sizeof(response)); Serial.print("Modem Info: "); Serial.println(response);
  sendATCommand("AT+CSQ", "OK", 1000, false, response, sizeof(response)); Serial.print("Signal Quality: "); Serial.println(response);
  sendATCommand("AT+CPIN?", "OK", 1000, false, response, sizeof(response)); Serial.print("SIM Status: "); Serial.println(response);
  sendATCommand("AT+CCID", "OK", 1000, false, response, sizeof(response)); Serial.print("CCID: "); Serial.println(response);
  sendATCommand("AT+COPS?", "OK", 1000, false, response, sizeof(response)); Serial.print("Operator: "); Serial.println(response);
  Serial.println("--------------------");
}


bool sendATCommand(const char* cmd, const char* expect, unsigned long timeout, bool debug, char* out_response, size_t out_len) {
  if (out_response) *out_response = '\0';
  
  if (cmd) { // If cmd is not null, send it
      if(debug) { Serial.print("[AT SEND] "); Serial.println(cmd); }
      modemSerial.println(cmd);
  }
  
  unsigned long start = millis();
  String res = "";
  
  while (millis() - start < timeout) {
    while (modemSerial.available()) {
      char c = modemSerial.read();
      res += c;
    }
    if (res.indexOf(expect) != -1) {
      if(debug) { Serial.print("[AT RECV] "); Serial.println(res); }
      if (out_response) {
          // Clean up response: remove command echo and OK/ERROR
          String clean_res = res;
          if(cmd) clean_res.replace(String(cmd), "");
          clean_res.replace("OK", "");
          clean_res.replace("ERROR", "");
          clean_res.trim();
          strncpy(out_response, clean_res.c_str(), out_len - 1);
          out_response[out_len - 1] = '\0';
      }
      return true;
    }
  }
  
  if(debug) {
      Serial.print("[DEBUG] Timeout waiting for: "); Serial.println(expect);
      Serial.print("[DEBUG] Received: "); Serial.println(res);
  }
  return false;
}
