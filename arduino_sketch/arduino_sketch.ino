
#include <SPI.h>
#include <SD.h>
#include <FS.h>

#define TINY_GSM_MODEM_SIM7600
#define TINY_GSM_USE_GPRS true

#include <TinyGsmClient.h>
#include <HardwareSerial.h>

// Your board hardware definitions
const int SD_CS_PIN = 5;

// Your modem hardware definitions
const int MODEM_TX = 16;
const int MODEM_RX = 17;
const int MODEM_BAUD = 115200;

// Your network settings
const char apn[] = "internet"; // APN for One NZ
const char gprsUser[] = "";
const char gprsPass[] = "";

// Server details
const char server[] = "cellular-data-streamer.web.app";
const int port = 443;
const char resource[] = "/api/upload";

// File upload settings
const int CHUNK_BUFFER_SIZE = 4096;

HardwareSerial modemSerial(1);
TinyGsm modem(modemSerial);

// --- UTILITY FUNCTIONS ---

// Helper function to send AT command and print to Serial
void sendATCommand(const char* cmd, bool debug = true) {
  if (debug) {
    Serial.print("[AT SEND] ");
    Serial.println(cmd);
  }
  modemSerial.println(cmd);
}

// Helper to wait for a specific response
bool waitForResponse(unsigned long timeout, const char* expected) {
  unsigned long start = millis();
  String response;
  while (millis() - start < timeout) {
    if (modemSerial.available()) {
      char c = modemSerial.read();
      response += c;
      if (response.endsWith(expected)) {
        if (String(expected) == "> ") {
           Serial.println("[DEBUG] Got > prompt.");
        }
        return true;
      }
    }
  }
  Serial.print("[DEBUG] Timeout waiting for: ");
  Serial.println(expected);
  Serial.print("[DEBUG] Received: ");
  Serial.println(response);
  return false;
}

// --- CORE FUNCTIONS ---

bool waitForModemReady() {
  Serial.println("Waiting for modem to be ready...");
  for (int i = 0; i < 30; i++) { // Wait up to 15 seconds
    if (modem.testAT(1000)) {
       SimStatus simStatus = modem.getSimStatus();
       if (simStatus == SIM_READY) {
          Serial.println("✅ Modem and SIM are ready.");
          return true;
       }
       Serial.print("SIM Status: ");
       Serial.println(simStatus);
    }
    delay(500);
  }
  return false;
}

bool waitForNetwork() {
    Serial.println("Waiting for network registration...");
    for (int i = 0; i < 30; i++) { // Wait up to 30 seconds
        int registrationStatus = modem.getRegistrationStatus();
        Serial.print("Network registration status: ");
        Serial.println(registrationStatus);
        if (registrationStatus == 1 || registrationStatus == 5) { // Registered on home or roaming
            Serial.println("✅ Registered on network.");
            return true;
        }
        delay(1000);
    }
    return false;
}

bool configureSSL() {
  Serial.println("🔧 Configuring SSL...");
  String cmd;

  // Set SSL version to allow TLS 1.2
  cmd = "AT+CSSLCFG=\"sslversion\",1,3";
  sendATCommand(cmd.c_str());
  if (!modem.waitResponse(3000, "OK")) {
    Serial.println("❌ Failed to set SSL version.");
    return false;
  }

  // Set authentication mode to not require server certificate validation (for development)
  cmd = "AT+CSSLCFG=\"authmode\",1,0";
  sendATCommand(cmd.c_str());
  if (!modem.waitResponse(3000, "OK")) {
    Serial.println("❌ Failed to set SSL auth mode.");
    return false;
  }
  
  // Enable HTTPS mode
  cmd = "AT+HTTPSSL=1";
  sendATCommand(cmd.c_str());
  if (!modem.waitResponse(3000, "OK")) {
    Serial.println("❌ Failed to enable HTTPS.");
    return false;
  }

  Serial.println("✅ SSL configured successfully.");
  return true;
}

bool openHttpsSession() {
  sendATCommand("AT+CHTTPSSTART");
  if (!waitForResponse(10000, "+CHTTPSSTART: 0")) {
      waitForResponse(10000, "OK"); // Clear buffer
      Serial.println("❌ Failed to start HTTPS service.");
      return false;
  }
  waitForResponse(1000, "OK");
  
  String cmd = "AT+CHTTPSOPSE=\"" + String(server) + "\"," + String(port);
  sendATCommand(cmd.c_str());

  if (!waitForResponse(20000, "+CHTTPSOPSE: 0")) {
    Serial.println("❌ Failed to open HTTPS session with server.");
    // Close the session on failure
    sendATCommand("AT+CHTTPSCLSE");
    waitForResponse(1000, "OK");
    sendATCommand("AT+CHTTPSSTOP");
    waitForResponse(1000, "OK");
    return false;
  }
  waitForResponse(1000, "OK");
  Serial.println("✅ HTTPS session opened.");
  return true;
}

bool closeHttpsSession() {
    sendATCommand("AT+CHTTPSCLSE");
    if(!waitForResponse(1000, "OK")) {
        Serial.println("Warning: Did not get OK on CHTTPCLSE.");
    }
    sendATCommand("AT+CHTTPSSTOP");
    if(!waitForResponse(1000, "OK")) {
        Serial.println("Warning: Did not get OK on CHTTPSTOP.");
    }
    Serial.println("✅ HTTPS session closed.");
    return true;
}

bool sendChunk(File& file, const char* filename, size_t totalSize, size_t chunkOffset) {
    uint8_t chunkBuffer[CHUNK_BUFFER_SIZE];
    size_t chunkSize = file.read(chunkBuffer, sizeof(chunkBuffer));
    if (chunkSize == 0) return false; // No more data to read

    String headers = "x-filename: " + String(filename) + "\r\n";
    headers += "x-chunk-offset: " + String(chunkOffset) + "\r\n";
    headers += "x-chunk-size: " + String(chunkSize) + "\r\n";
    headers += "x-total-size: " + String(totalSize) + "\r\n";
    headers += "Content-Type: application/octet-stream\r\n";

    int headerLength = headers.length();
    
    // Command: AT+CHTTPSPOST=<uri>,<header_len>,<content_len>,<timeout_ms>
    String postCmd = "AT+CHTTPSPOST=\"" + String(resource) + "\"," + String(headerLength) + "," + String(chunkSize) + ",10000";
    sendATCommand(postCmd.c_str());

    if (!waitForResponse(2000, "> ")) {
        Serial.println("❌ Modem did not respond to POST command. Aborting.");
        return false;
    }

    // Send headers then content
    modemSerial.print(headers);
    modemSerial.write(chunkBuffer, chunkSize);
    modemSerial.flush();

    // Check for success response +CHTTPS: POST,200
    if (!waitForResponse(15000, "+CHTTPS: POST,200")) {
        Serial.println("❌ Failed to get 200 OK from server for chunk.");
        return false;
    }
    
    waitForResponse(1000, "OK"); // Consume final OK
    
    float progress = (float)(chunkOffset + chunkSize) / totalSize * 100;
    Serial.printf("  ...chunk sent. Progress: %.2f%%\n", progress);

    return true;
}


void uploadFile(const char* filename) {
    File file = SD.open(filename, FILE_READ);
    if (!file) {
        Serial.println("❌ Failed to open file for reading.");
        return;
    }

    size_t fileSize = file.size();
    Serial.printf("📤 Preparing to upload %s (%d bytes)\n", filename, fileSize);

    if (!openHttpsSession()) {
        file.close();
        return;
    }

    size_t offset = 0;
    bool success = true;
    while (offset < fileSize) {
        if (!sendChunk(file, filename, fileSize, offset)) {
            success = false;
            break;
        }
        offset = file.position();
    }

    if (success) {
        Serial.println("✅ File upload completed successfully.");
    } else {
        Serial.println("❌ File upload failed.");
    }

    file.close();
    closeHttpsSession();
}

// --- ARDUINO SETUP & LOOP ---

void setup() {
  Serial.begin(115200);
  while (!Serial);

  Serial.println("🔌 Booting...");

  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("❌ SD card mount failed. Halting.");
    while (1);
  }
  Serial.println("✅ SD card ready.");

  Serial.println("Initializing modem...");
  modemSerial.begin(MODEM_BAUD, SERIAL_8N1, MODEM_RX, MODEM_TX);

  if (!waitForModemReady()) {
      Serial.println("❌ Modem not responding. Halting.");
      while(1);
  }
  
  Serial.println("--- Modem Status ---");
  modem.sendAT("+CIMI");
  String imei = "";
  modem.waitResponse(1000, imei);
  Serial.println("IMEI: " + imei);
  Serial.println("Signal Quality: " + String(modem.getSignalQuality()));
  Serial.println("SIM Status: " + String(modem.getSimStatus()));
  Serial.println("CCID: " + modem.getSimCCID());
  Serial.println("Operator: " + modem.getOperator());
  Serial.println("--------------------");

  Serial.println("📡 Connecting to network...");
  if (!waitForNetwork()) {
      Serial.println("❌ Failed to register on network. Halting.");
      while(1);
  }

  if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
      Serial.println("❌ GPRS connection failed.");
      while(1);
  }
  Serial.println("✅ GPRS connected.");

  String localIP = modem.getLocalIP();
  Serial.println("Local IP: " + localIP);
  
  uploadFile("/sigma2.wav");
}

void loop() {
  // All logic is in setup for this one-shot example
  delay(10000);
}
