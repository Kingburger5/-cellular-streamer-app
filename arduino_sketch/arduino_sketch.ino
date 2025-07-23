/**
 * @fileOverview A cellular data logger sketch for ESP32 and SIM7600.
 * This sketch reads a file from an SD card, chunks it, and uploads it
 * to a web server over a cellular HTTPS connection.
 */
#define TINY_GSM_MODEM_SIM7600
#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <HardwareSerial.h>
#include <TinyGsmClient.h>

// --- Pin Definitions ---
// SD Card SPI pins
#define SD_CS_PIN 5
#define SD_SCK_PIN 18
#define SD_MOSI_PIN 23
#define SD_MISO_PIN 19

// Modem Serial pins (using Hardware Serial 2)
#define MODEM_TX_PIN 17
#define MODEM_RX_PIN 16
#define MODEM_PWRKEY_PIN 4

// --- Configuration ---
// Server details for file upload
const char server[] = "6000-firebase-studio-1753223410587.cluster-73qgvk7hjjadkrjeyexca5ivva.cloudworkstations.dev";
const char resource[] = "/api/upload";
const int port = 443;

// APN details for cellular connection (replace with your provider's APN)
const char apn[] = "internet"; // Example for One NZ, change if needed
const char gprsUser[] = "";
const char gprsPass[] = "";

// File to upload from the SD card
const char* filename = "/sigma2.wav";

// Size of each chunk to upload in bytes
const size_t CHUNK_SIZE = 1024 * 32; // 32 KB

// --- Global Objects ---
HardwareSerial modemSerial(2);
TinyGsm modem(modemSerial);
TinyGsmClientSecure client(modem);
SPIClass spi(HSPI);

// --- Debug Utilities ---
#define DEBUG_PRINT(x) Serial.println(x)

/**
 * @brief Sends an AT command and waits for a specific response.
 * @param cmd The AT command to send.
 * @param timeout The time to wait for a response in milliseconds.
 * @param okResp The expected successful response.
 * @param errResp An optional error response string.
 * @return True if the expected response is received, false otherwise.
 */
bool sendATCommandCheck(const char* cmd, unsigned long timeout, const char* okResp, const char* errResp = nullptr) {
  modem.sendAT(cmd);
  String response;
  if (modem.waitResponse(timeout, response) == 1) {
    if (response.indexOf(okResp) != -1) {
      return true;
    }
  }
  if (errResp != nullptr && response.indexOf(errResp) != -1) {
    DEBUG_PRINT("AT Command Error Response: " + response);
  } else if (errResp == nullptr) {
    DEBUG_PRINT("AT Command Unexpected Response: " + response);
  }
  return false;
}

/**
 * @brief Prints the status of the modem including IMEI, signal quality, etc.
 */
void printModemStatus() {
  DEBUG_PRINT("--- Modem Status ---");

  String imei = modem.getIMEI();
  DEBUG_PRINT("IMEI: " + imei);

  int csq = modem.getSignalQuality();
  DEBUG_PRINT("Signal Quality: " + String(csq));

  SimStatus sim = modem.getSimStatus();
  DEBUG_PRINT("SIM Status: " + String(sim));

  String ccid = modem.getSimCCID();
  DEBUG_PRINT("CCID: " + ccid);

  String op = modem.getOperator();
  DEBUG_PRINT("Operator: " + op);

  DEBUG_PRINT("--------------------");
}

/**
 * @brief Waits for the modem to become ready by repeatedly testing AT commands.
 * @return True if the modem becomes ready, false if it times out.
 */
bool waitForModemReady() {
  for (int i = 0; i < 20; i++) {
    if (modem.testAT(1000)) {
       SimStatus simStatus = modem.getSimStatus();
       if (simStatus == SIM_READY) {
          return true;
       }
    }
    delay(500);
  }
  return false;
}

/**
 * @brief Waits for the modem to register on the cellular network.
 * @return True if registered successfully, false otherwise.
 */
bool waitForNetwork() {
  DEBUG_PRINT("Waiting for network registration...");
  for (int i = 0; i < 30; i++) { // Wait for up to 30 seconds
    int regStatus = modem.getRegistrationStatus();
    DEBUG_PRINT("Network registration status: " + String(regStatus));
    if (regStatus == 1 || regStatus == 5) { // Registered home or roaming
      DEBUG_PRINT("✅ Registered on network.");
      return true;
    }
    delay(1000);
  }
  DEBUG_PRINT("❌ Network registration failed.");
  return false;
}

/**
 * @brief Sets up the modem, including serial communication and initialization.
 */
void setupModem() {
  // Set console baud rate
  Serial.begin(115200);

  // Set-up modem serial communication
  modemSerial.begin(115200, SERIAL_8N1, MODEM_RX_PIN, MODEM_TX_PIN);

  DEBUG_PRINT("Initializing modem...");
  if (!modem.init()) {
      DEBUG_PRINT("Modem initialization failed. Halting.");
      while(true);
  }

  // Wait for modem to be responsive
  int retry = 0;
  while (!modem.testAT(1000)) {
      DEBUG_PRINT("Modem not responding, retrying...");
      delay(1000);
      if (retry++ > 20) {
          DEBUG_PRINT("Modem did not respond after multiple retries. Halting.");
          while(true);
      }
  }

  DEBUG_PRINT("Modem initialized.");
}


/**
 * @brief Initializes the SD card reader.
 */
void setupSdCard() {
  spi.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
  if (!SD.begin(SD_CS_PIN, spi)) {
    DEBUG_PRINT("❌ SD Card initialization failed. Halting.");
    while (true)
      ;
  }
  DEBUG_PRINT("✅ SD card initialized.");
}

/**
 * @brief Opens a secure HTTPS session with the server.
 * This function encapsulates the AT commands needed to start an HTTPS session.
 * @return True on success, false on failure.
 */
bool openHttpsSession() {
    DEBUG_PRINT("[AT SEND] AT+CHTTPSSTART");
    if (!sendATCommandCheck("AT+CHTTPSSTART", 10000, "+CHTTPSSTART: 0", "ERROR")) {
        return false;
    }

    String cmd = "AT+CHTTPSOPSE=\"" + String(server) + "\"," + String(port);
    DEBUG_PRINT("[AT SEND] " + cmd);
    if (!sendATCommandCheck(cmd.c_str(), 20000, "+CHTTPSOPSE: 0", "ERROR")) {
        return false;
    }
    
    return true;
}

/**
 * @brief Closes the currently open HTTPS session.
 */
void closeHttpsSession() {
    DEBUG_PRINT("[AT SEND] AT+CHTTPSCLSE");
    sendATCommandCheck("AT+CHTTPSCLSE", 10000, "OK", "ERROR");

    DEBUG_PRINT("[AT SEND] AT+CHTTPSSTOP");
    sendATCommandCheck("AT+CHTTPSSTOP", 1000, "OK", "ERROR");
}

/**
 * @brief Sends a single chunk of a file over HTTPS.
 * @param file The file object to read from.
 * @param fileIdentifier A unique identifier for the file being uploaded.
 * @param chunkOffset The offset within the file where this chunk starts.
 * @param chunkSize The size of the chunk to send.
 * @param totalSize The total size of the file.
 * @return True on success, false on failure.
 */
bool sendChunk(fs::File& file, const char* fileIdentifier, size_t chunkOffset, size_t chunkSize, size_t totalSize) {
  for (int attempt = 1; attempt <= 3; attempt++) {
    DEBUG_PRINT("  Attempt " + String(attempt) + "/3 to send chunk at offset " + String(chunkOffset) + "...");
    
    if (!openHttpsSession()) {
        DEBUG_PRINT("    ...connection failed.");
        closeHttpsSession(); // Ensure cleanup on failure
        delay(2000); // Wait before retrying
        continue;
    }

    DEBUG_PRINT("    ...connection successful. Preparing request...");

    // 1. Set request headers
    String headers = "Content-Type: application/octet-stream\r\n";
    headers += "X-Filename: " + String(fileIdentifier) + "\r\n";
    headers += "X-Chunk-Offset: " + String(chunkOffset) + "\r\n";
    headers += "X-Chunk-Size: " + String(chunkSize) + "\r\n";
    headers += "X-Total-Size: " + String(totalSize) + "\r\n";

    String cmd = "AT+CHTTPSSEND=2," + String(strlen(resource));
    if (!sendATCommandCheck(cmd.c_str(), 1000, ">")) return false;
    modem.stream.print(resource);
    modem.waitResponse();

    cmd = "AT+CHTTPSSEND=1," + String(headers.length());
    if (!sendATCommandCheck(cmd.c_str(), 1000, ">")) return false;
    modem.stream.print(headers);
    modem.waitResponse();

    // 2. Send the chunk data
    cmd = "AT+CHTTPSSEND=0," + String(chunkSize);
    if (!sendATCommandCheck(cmd.c_str(), 1000, ">")) return false;

    uint8_t chunkBuffer[512];
    size_t bytesSent = 0;
    while (bytesSent < chunkSize) {
      size_t toRead = min((size_t)sizeof(chunkBuffer), chunkSize - bytesSent);
      size_t bytesRead = file.read(chunkBuffer, toRead);
      if (bytesRead == 0) break;
      modem.stream.write(chunkBuffer, bytesRead);
      bytesSent += bytesRead;
    }
    
    String response;
    if (modem.waitResponse(60000, response) == 1) { // Long timeout for upload
      if (response.indexOf("+CHTTPSSEND: 0") != -1) {
        DEBUG_PRINT("    ...chunk sent successfully.");
        
        // 3. Check server response
        sendATCommandCheck("AT+CHTTPSRECV=1000", 20000, "OK"); // Read response
        
        closeHttpsSession();
        return true;
      }
    }
    
    DEBUG_PRINT("    ...chunk send failed.");
    closeHttpsSession(); // Clean up on failure
    delay(2000); // Wait before retrying
  }

  return false;
}


/**
 * @brief Main setup function. Initializes modem, SD card, and starts the upload process.
 */
void setup() {
  setupModem();

  if (!waitForModemReady()) {
      DEBUG_PRINT("❌ Modem not ready after waiting. Halting.");
      while(true);
  }
  DEBUG_PRINT("✅ Modem and SIM are ready.");

  printModemStatus();

  DEBUG_PRINT("📡 Connecting to network...");
  if (!waitForNetwork()) {
      while(true);
  }

  if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
    DEBUG_PRINT("❌ GPRS connection failed.");
    while (true);
  }
  DEBUG_PRINT("✅ GPRS connected.");

  String ip = modem.getLocalIP();
  DEBUG_PRINT("Local IP: " + ip);

  setupSdCard();

  File file = SD.open(filename, FILE_READ);
  if (!file) {
    DEBUG_PRINT("❌ Failed to open file: " + String(filename));
    return;
  }

  size_t fileSize = file.size();
  DEBUG_PRINT("📤 Preparing to upload " + String(filename) + " (" + String(fileSize) + " bytes)");

  size_t offset = 0;
  while (offset < fileSize) {
    size_t chunkSize = min(CHUNK_SIZE, fileSize - offset);
    file.seek(offset);

    if (sendChunk(file, filename, offset, chunkSize, fileSize)) {
      DEBUG_PRINT("✅ Chunk uploaded successfully. Offset: " + String(offset));
      offset += chunkSize;
    } else {
      DEBUG_PRINT("❌ Failed to upload chunk at offset " + String(offset) + " after 3 retries. Aborting.");
      break;
    }
  }

  file.close();
  DEBUG_PRINT("🏁 Task finished. Entering idle loop.");
}

/**
 * @brief Main loop. Does nothing as the primary logic is in setup().
 */
void loop() {
  // Keep the main loop empty, as this is a one-shot task.
  delay(10000);
}
