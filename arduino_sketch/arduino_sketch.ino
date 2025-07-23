
// Define the modem type before including any TinyGSM libraries.
#define TINY_GSM_MODEM_SIM7600
// Enable verbose debug prints to the Serial Monitor.
#define TINY_GSM_DEBUG Serial

#include <HardwareSerial.h>
#include <TinyGsmClient.h>
#include <SD.h>
#include <SPI.h>

// --- Pin Definitions ---
// Hardware Serial for modem communication.
#define MODEM_TX 17
#define MODEM_RX 16
// Chip select pin for the SD card.
#define SD_CS 5

// --- Modem & Network Configuration ---
#define MODEM_BAUD 115200
const char apn[] = "internet"; // APN for One NZ
const char gprsUser[] = "";
const char gprsPass[] = "";

// --- Server & Upload Configuration ---
const char server[] = "6000-firebase-studio-1753223410587.cluster-73qgvk7hjjadkrjeyexca5ivva.cloudworkstations.dev";
const int port = 443;
const char resource[] = "/api/upload";
#define CHUNK_SIZE 4096 // Use a larger chunk size for faster uploads.

// --- Global Objects ---
HardwareSerial XCOM(1); // Use UART1 for the modem.
// Note: We don't use TinyGsmClient for uploads, but it's useful for some status checks.
TinyGsm modem(XCOM);

// --- Forward Declarations ---
bool sendATCommand(const char* cmd, unsigned long timeout, const char* expected_response);
bool sendATCommand(const __FlashStringHelper* cmd, unsigned long timeout, const char* expected_response);


// =================================================================
// --- Main Setup and Loop ---
// =================================================================

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println(F("? Booting..."));

  // --- Initialize SD Card ---
  if (!SD.begin(SD_CS)) {
    Serial.println(F("? SD card failed or not present. Halting."));
    while (true);
  }
  Serial.println(F("? SD card ready."));

  // --- Initialize Modem ---
  Serial.println(F("? Initializing modem..."));
  XCOM.begin(MODEM_BAUD, SERIAL_8N1, MODEM_RX, MODEM_TX);
  delay(1000);

  Serial.println(F("? Waiting for modem to be ready..."));
  unsigned long start = millis();
  while (millis() - start < 10000) {
    if (modem.testAT()) {
      Serial.println(F("? Modem is ready."));
      printModemStatus();
      if (waitForNetwork()) {
        if (manualGprsConnect()) {
           uploadFile("/sigma2.wav");
        } else {
           Serial.println(F("? GPRS connection failed. Halting."));
        }
      } else {
        Serial.println(F("? Network registration failed. Halting."));
      }
      return; // Exit setup
    }
    delay(500);
  }
  Serial.println(F("? Modem not responding. Halting."));
}

void loop() {
  // All logic is in setup() for this single-task sketch.
}

// =================================================================
// --- Core Functions ---
// =================================================================

bool waitForNetwork() {
  Serial.println(F("? Waiting for network registration..."));
  unsigned long start = millis();
  while (millis() - start < 60000L) { // 60-second timeout
    int regStatus = modem.getRegistrationStatus();
    Serial.print(F("Network registration status: "));
    Serial.println(regStatus);
    if (regStatus == 1 || regStatus == 5) {
      Serial.println(F("? Registered on network."));
      return true;
    }
    delay(2000);
  }
  return false;
}

bool manualGprsConnect() {
    Serial.println(F("? Connecting to GPRS..."));
    
    // Set the APN
    String cmd = "AT+CGDCONT=1,\"IP\",\"";
    cmd += apn;
    cmd += "\"";
    if (!sendATCommand(cmd.c_str(), 5000, "OK")) {
        return false;
    }

    // Activate GPRS context
    if (!sendATCommand(F("AT+CGACT=1,1"), 10000, "OK")) {
        Serial.println(F("? Failed to activate GPRS context."));
        return false;
    }

    // Check for IP address
    if (!sendATCommand(F("AT+CGPADDR=1"), 10000, "+CGPADDR: 1,")) {
        Serial.println(F("? Failed to get IP address."));
        return false;
    }

    Serial.println(F("? GPRS Connected."));
    return true;
}


bool openHttpsSession() {
  if (!sendATCommand(F("AT+CHTTPSSTART"), 20000, "OK")) {
    return false;
  }

  String cmd = "AT+CHTTPSOPSE=\"";
  cmd += server;
  cmd += "\",";
  cmd += port;
  
  if (!sendATCommand(cmd.c_str(), 20000, "+CHTTPSOPSE: 0")) {
     Serial.println(F("? Failed to open HTTPS session."));
     return false;
  }
  
  cmd = "AT+CHTTPSPARA=\"URL\",\"";
  cmd += resource;
  cmd += "\"";
  if (!sendATCommand(cmd.c_str(), 20000, "OK")) {
      Serial.println(F("? Failed to set URL parameter."));
      return false;
  }

  return true;
}


bool setRequestHeaders(const char* filename, size_t offset, size_t chunkSize, size_t totalSize) {
    String cmd;

    cmd = "AT+CHTTPSPARA=\"USERDATA\",\"Content-Type: application/octet-stream\"";
    if (!sendATCommand(cmd.c_str(), 5000, "OK")) return false;

    cmd = "AT+CHTTPSPARA=\"USERDATA\",\"X-Filename: ";
    cmd += filename;
    cmd += "\"";
    if (!sendATCommand(cmd.c_str(), 5000, "OK")) return false;
    
    cmd = "AT+CHTTPSPARA=\"USERDATA\",\"X-Chunk-Offset: ";
    cmd += offset;
    cmd += "\"";
    if (!sendATCommand(cmd.c_str(), 5000, "OK")) return false;

    cmd = "AT+CHTTPSPARA=\"USERDATA\",\"X-Chunk-Size: ";
    cmd += chunkSize;
    cmd += "\"";
    if (!sendATCommand(cmd.c_str(), 5000, "OK")) return false;

    cmd = "AT+CHTTPSPARA=\"USERDATA\",\"X-Total-Size: ";
    cmd += totalSize;
    cmd += "\"";
    if (!sendATCommand(cmd.c_str(), 5000, "OK")) return false;

    return true;
}

void closeHttpsSession() {
    sendATCommand(F("AT+CHTTPSCLSE"), 5000, "OK");
    sendATCommand(F("AT+CHTTPSSTOP"), 10000, "OK");
}

void uploadFile(const char* filename) {
  File file = SD.open(filename);
  if (!file) {
    Serial.println(F("? Failed to open file for reading."));
    return;
  }

  size_t totalSize = file.size();
  Serial.print(F("? Preparing to upload "));
  Serial.print(filename);
  Serial.print(F(" ("));
  Serial.print(totalSize);
  Serial.println(F(" bytes)"));

  size_t offset = 0;
  while (offset < totalSize) {
    size_t chunkSize = min((size_t)CHUNK_SIZE, totalSize - offset);

    bool chunkSent = false;
    for (int attempt = 1; attempt <= 3; attempt++) {
      Serial.print(F("  Attempt "));
      Serial.print(attempt);
      Serial.print(F("/3 to send chunk at offset "));
      Serial.print(offset);
      Serial.println(F("..."));
      
      if (sendChunk(file, offset, chunkSize)) {
        chunkSent = true;
        break; // Success, exit retry loop
      } else {
        Serial.println(F("? Retrying chunk in 5 seconds..."));
        delay(5000);
      }
    }

    if (!chunkSent) {
      Serial.println(F("? Failed to upload chunk after 3 attempts. Aborting."));
      file.close();
      return;
    }
    offset += chunkSize;
  }

  Serial.println(F("? File upload completed successfully."));
  file.close();
}

bool sendChunk(File& file, size_t offset, size_t chunkSize) {
  if (!openHttpsSession()) {
      closeHttpsSession();
      return false;
  }
  
  if (!setRequestHeaders(file.name(), offset, chunkSize, file.size())) {
      Serial.println(F("? Failed to set request headers."));
      closeHttpsSession();
      return false;
  }

  // Set the content length for the POST data
  String cmd = "AT+CHTTPSPARA=\"Content-Length\",";
  cmd += chunkSize;
  if (!sendATCommand(cmd.c_str(), 5000, "OK")) {
    Serial.println(F("? Failed to set Content-Length."));
    closeHttpsSession();
    return false;
  }

  // Send POST request, wait for the ">" prompt
  if (!sendATCommand(F("AT+CHTTPSPOST=0"), 20000, ">")) {
      Serial.println(F("? Failed to start POST request."));
      closeHttpsSession();
      return false;
  }

  // Read chunk from SD and send to modem
  uint8_t buffer[256];
  size_t bytesSent = 0;
  file.seek(offset);
  unsigned long chunkStartTime = millis();

  while (bytesSent < chunkSize) {
      size_t bytesToRead = min((size_t)sizeof(buffer), chunkSize - bytesSent);
      size_t bytesRead = file.read(buffer, bytesToRead);
      if (bytesRead == 0) break;
      XCOM.write(buffer, bytesRead);
      bytesSent += bytesRead;
  }
  
  // Wait for the server response
  if (!sendATCommand(nullptr, 30000, "+CHTTPSPOST: 0,200")) {
      Serial.println(F("? Chunk upload failed. Server did not return 200 OK."));
      closeHttpsSession();
      return false;
  }
  
  Serial.println(F("? Chunk sent successfully."));
  closeHttpsSession();
  return true;
}

// =================================================================
// --- Utility and Debug Functions ---
// =================================================================

void printModemStatus() {
  Serial.println(F("--- Modem Status ---"));
  String imei = modem.getIMEI();
  sendATCommand(F("AT+GSN"), 1000, "OK");
  Serial.print(F("IMEI: "));
  Serial.println(imei);

  int csq = modem.getSignalQuality();
  sendATCommand(F("AT+CSQ"), 1000, "OK");
  Serial.print(F("Signal Quality: "));
  Serial.println(csq);

  int simStatus = modem.getSimStatus();
  sendATCommand(F("AT+CPIN?"), 1000, "OK");
  Serial.print(F("SIM Status: "));
  Serial.println(simStatus);

  String ccid = modem.getSimCCID();
  sendATCommand(F("AT+CCID"), 1000, "OK");
  Serial.print(F("CCID: "));
  Serial.println(ccid);

  String op = modem.getOperator();
  sendATCommand(F("AT+COPS?"), 5000, "OK");
  Serial.print(F("Operator: "));
  Serial.println(op);
  Serial.println(F("--------------------"));
}

// Helper function to send an AT command and wait for a specific response.
bool sendATCommand(const char* cmd, unsigned long timeout, const char* expected_response) {
    String res;
    if (cmd) {
        Serial.print(F("[AT SEND] "));
        Serial.println(cmd);
        XCOM.println(cmd);
    }
    
    unsigned long start = millis();
    while (millis() - start < timeout) {
        while (XCOM.available()) {
            res = XCOM.readString();
            Serial.print(F("[AT RECV] "));
            Serial.println(res);
            if (res.indexOf(expected_response) != -1) {
                return true;
            }
        }
    }
    Serial.println(F("[AT RECV TIMEOUT]"));
    return false;
}

// Overloaded version for FlashStringHelper
bool sendATCommand(const __FlashStringHelper* cmd, unsigned long timeout, const char* expected_response) {
    String res;
    if (cmd) {
        Serial.print(F("[AT SEND] "));
        Serial.println(cmd);
        XCOM.println(cmd);
    }

    unsigned long start = millis();
    while (millis() - start < timeout) {
        while (XCOM.available()) {
            res = XCOM.readString();
            Serial.print(F("[AT RECV] "));
            Serial.println(res);
            if (res.indexOf(expected_response) != -1) {
                return true;
            }
        }
    }
    Serial.println(F("[AT RECV TIMEOUT]"));
    return false;
}

    