
// Cellular Data Streamer for SIM7600G-H
// For more information, please visit:
// https://firebase.google.com/docs/studio

#include <Arduino.h>
#include <HardwareSerial.h>
#include <SD.h>
#include <SPI.h>

#define TINY_GSM_MODEM_SIM7600
#include <TinyGsmClient.h>

// Pin definitions for your ESP32 board
#define SD_CS_PIN 5
#define MODEM_RX_PIN 16
#define MODEM_TX_PIN 17
#define MODEM_BAUD 115200

// Server configuration
const char server[] = "6000-firebase-studio-1753223410587.cluster-73qgvk7hjjadkrjeyexca5ivva.cloudworkstations.dev";
const char resource[] = "/api/upload";
const int port = 443;
const char apn[] = "internet";  // APN for One NZ
const char gprsUser[] = "";
const char gprsPass[] = "";

// Serial port for modem communication
HardwareSerial modemSerial(1);

// TinyGSM modem instance
TinyGsm modem(modemSerial);

// --- Function Prototypes ---
void setupModem();
bool sendATCommand(const char *cmd, unsigned long timeout, const char *expected_response);
bool sendATCommandCheck(const char *cmd, unsigned long timeout, const char *okResp, const char *errResp);
bool openHttpsSession();
void closeHttpsSession();
bool sendChunk(File &file, const char *filename, size_t offset, size_t totalSize);
void sendFileChunks(const char *filename);
void printModemStatus();
bool waitForModemReady();
bool waitForNetwork();

// --- Setup ---
void setup() {
  Serial.begin(115200);
  delay(10);

  Serial.println("? Booting...");

  // Initialize SD card
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("? SD Card initialization failed!");
    while (1)
      ;
  }
  Serial.println("? SD card ready.");

  // Initialize modem
  setupModem();
  if (!waitForModemReady()) {
    Serial.println("? Modem not responding. Halting.");
    while(1);
  }

  Serial.println("? Modem and SIM are ready.");

  printModemStatus();
  
  Serial.println("? Connecting to network...");

  if (!waitForNetwork()) {
      Serial.println("? Network registration failed.");
      return;
  }
  
  if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
    Serial.println("? GPRS connection failed.");
    return;
  }

  Serial.println("? GPRS connected.");
  Serial.print("Local IP: ");
  Serial.println(modem.getLocalIP());

  // Main application logic
  sendFileChunks("/sigma2.wav");

  // Keep alive
  Serial.println("? Task finished. Entering idle loop.");
}

void loop() {
  // Keep the main loop clean, all logic is in setup() for this example.
  delay(10000);
}


// --- Modem & Communication Functions ---

void setupModem() {
  Serial.println("Initializing modem...");
  modemSerial.begin(MODEM_BAUD, SERIAL_8N1, MODEM_RX_PIN, MODEM_TX_PIN);
  delay(3000);
  
  if (modem.testAT(10000)) {
     Serial.println("? Modem restarted.");
  }
}

void printModemStatus() {
  Serial.println("--- Modem Status ---");
  
  String imei = modem.getIMEI();
  Serial.println("IMEI: " + imei);

  int signalQuality = modem.getSignalQuality();
  Serial.println("Signal Quality: " + String(signalQuality));
  
  int simStatus = modem.getSimStatus();
  Serial.println("SIM Status: " + String(simStatus));

  String ccid = modem.getSimCCID();
  Serial.println("CCID: " + ccid);
  
  String operatorName = modem.getOperator();
  Serial.println("Operator: " + operatorName);
  
  Serial.println("--------------------");
}

bool waitForModemReady() {
    Serial.println("Waiting for modem to be ready...");
    for (int i = 0; i < 30; i++) {
        if (modem.testAT(1000)) {
            if (modem.getSimStatus() == SIM_READY) {
                return true;
            }
        }
        delay(1000);
    }
    return false;
}

bool waitForNetwork() {
    Serial.println("Waiting for network registration...");
    for (int i = 0; i < 30; i++) {
        int regStatus = modem.getRegistrationStatus();
        Serial.println("Network registration status: " + String(regStatus));
        if (regStatus == 1 || regStatus == 5) { // Registered home or roaming
            Serial.println("? Registered on network.");
            return true;
        }
        delay(2000);
    }
    return false;
}


String sendATCommand(const char* cmd, unsigned long timeout) {
    Serial.print("[AT SEND] ");
    Serial.println(cmd);
    
    modemSerial.println(cmd);
    
    String response = "";
    unsigned long startTime = millis();
    while (millis() - startTime < timeout) {
        if (modemSerial.available()) {
            response += (char)modemSerial.read();
        }
    }
    
    if (response == "") {
        Serial.println("[DEBUG] Timeout waiting for: any response");
    }
    Serial.print("[AT RECV] ");
    Serial.println(response);
    return response;
}


bool sendATCommandCheck(const char* cmd, unsigned long timeout, const char* okResp, const char* errResp = "ERROR") {
    Serial.print("[AT SEND] ");
    Serial.println(cmd);
    
    modemSerial.println(cmd);
    
    String response;
    if (modem.waitResponse(timeout, response) != 1) {
        Serial.println("[DEBUG] Timeout waiting for: " + String(okResp));
        Serial.println("[DEBUG] Received: " + response);
        return false;
    }

    if (response.indexOf(okResp) != -1) {
        return true;
    }
    
    if (response.indexOf(errResp) != -1) {
        Serial.println("[DEBUG] Received error response: " + response);
        return false;
    }

    Serial.println("[DEBUG] Unexpected response: " + response);
    return false;
}


bool openHttpsSession() {
  if (!sendATCommandCheck("AT+CHTTPSSTART", 20000, "OK")) {
    return false;
  }
  
  String cmd = "AT+CHTTPSOPSE=\"" + String(server) + "\"," + String(port);
  if (!sendATCommandCheck(cmd.c_str(), 20000, "+CHTTPSOPSE: 0")) {
      Serial.println("? Failed to open HTTPS session with server.");
      return false;
  }
  return true;
}

void closeHttpsSession() {
    sendATCommandCheck("AT+CHTTPSCLSE", 10000, "OK");
    sendATCommandCheck("AT+CHTTPSSTOP", 10000, "OK");
}

bool sendChunk(File &file, const char *filename, size_t offset, size_t totalSize) {
    
    const size_t maxChunkSize = 4096;
    uint8_t chunkBuffer[maxChunkSize];

    size_t chunkSize = file.size() - offset;
    if (chunkSize > maxChunkSize) {
        chunkSize = maxChunkSize;
    }
    
    size_t bytesRead = file.read(chunkBuffer, chunkSize);
    if (bytesRead != chunkSize) {
       Serial.println("? File read error.");
       return false;
    }

    if (!openHttpsSession()) {
        return false;
    }

    // Prepare headers
    String headers = "x-filename: " + String(filename) + "\r\n";
    headers += "x-chunk-offset: " + String(offset) + "\r\n";
    headers += "x-chunk-size: " + String(chunkSize) + "\r\n";
    headers += "x-total-size: " + String(totalSize) + "\r\n";
    headers += "Content-Type: application/octet-stream\r\n";
    
    String postCmd = "AT+CHTTPSPOST=\"" + String(resource) + "\"," + String(headers.length()) + "," + String(chunkSize) + ",10000";

    if (!sendATCommandCheck(postCmd.c_str(), 10000, ">")) {
         Serial.println("? Modem did not respond to POST command. Aborting.");
         closeHttpsSession();
         return false;
    }

    // Send headers and chunk data
    modemSerial.print(headers);
    modemSerial.write(chunkBuffer, chunkSize);

    // Wait for response after sending data
    String response;
    modem.waitResponse(20000, response);
    Serial.println("[POST RECV] " + response);
    
    closeHttpsSession();

    if (response.indexOf("+CHTTPS: POST,20") != -1) { // 200 or 201
        return true;
    } else {
        Serial.println("? Chunk upload failed with response: " + response);
        return false;
    }
}


void sendFileChunks(const char *filename) {
  File file = SD.open(filename);
  if (!file) {
    Serial.println("? Failed to open file for reading");
    return;
  }

  size_t totalSize = file.size();
  Serial.println("? Preparing to upload " + String(filename) + " (" + String(totalSize) + " bytes)");

  size_t offset = 0;
  int retryCount = 0;
  const int maxRetries = 3;

  while (offset < totalSize) {
    Serial.println("  Attempt " + String(retryCount + 1) + "/" + String(maxRetries) + " to send chunk at offset " + String(offset) + "...");
    
    file.seek(offset); // Ensure file pointer is at the correct position
    
    if (sendChunk(file, filename, offset, totalSize)) {
      offset += 4096; // This should be based on actual bytes read
      retryCount = 0; // Reset retry count on success
    } else {
      Serial.println("    ...connection failed.");
      retryCount++;
      if (retryCount >= maxRetries) {
        Serial.println("? Failed to upload chunk at offset " + String(offset) + " after " + String(maxRetries) + " retries. Aborting.");
        file.close();
        return;
      }
    }
  }

  Serial.println("? File upload successful!");
  file.close();
}
