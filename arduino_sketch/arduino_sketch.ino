// =================================================================
//
// TINY GSM - AT-COMMAND BASED FILE UPLOADER
//
// This sketch is designed for maximum reliability by using direct
// AT commands for all critical network and HTTPS operations.
//
// =================================================================

// Step 1: Define Modem & Pins
#define TINY_GSM_MODEM_SIM7600
#define MODEM_TX 17
#define MODEM_RX 16
#define MODEM_BAUD 115200
#define SD_CS 5
#define TINY_GSM_DEBUG Serial // Make TinyGSM library debugging available

// Step 2: Network and Server Configuration
const char apn[] = "internet"; // APN for One NZ
const char gprsUser[] = "";
const char gprsPass[] = "";
const char server[] = "6000-firebase-studio-1753223410587.cluster-73qgvk7hjjadkrjeyexca5ivva.cloudworkstations.dev";
const char endpoint[] = "/api/upload";
const int port = 443;
const size_t CHUNK_SIZE = 4096; // 4KB chunks for efficient upload

// Step 3: Library Includes
#include <HardwareSerial.h>
#include <TinyGsmClient.h>
#include <SD.h>
#include <SPI.h>

// Step 4: Global Objects
HardwareSerial modemSerial(1);
TinyGsm modem(modemSerial);

// =================================================================
//
// SETUP & MAIN LOOP
//
// =================================================================

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println(F("? Booting..."));

  if (!SD.begin(SD_CS)) {
    Serial.println(F("? SD card failed to initialize. Halting."));
    while (true);
  }
  Serial.println(F("? SD card ready."));

  setupModem();
  printModemStatus();

  if (!waitForNetwork()) {
    Serial.println(F("? Failed to register on network. Halting."));
    while (true);
  }

  if (!manualGprsConnect()) {
    Serial.println(F("? GPRS connection failed. Halting."));
    while (true);
  }

  Serial.printf("? Preparing to upload %s (%d bytes)\n", "/sigma2.wav", getFileSize("/sigma2.wav"));
  uploadFile("/sigma2.wav");

  Serial.println(F("? Task finished. Entering idle loop."));
}

void loop() {
  // Kept empty as all work is done in setup
}

// =================================================================
//
// CORE FUNCTIONS
//
// =================================================================

/**
 * @brief Initializes the modem and waits for it to become responsive.
 */
void setupModem() {
  Serial.println(F("? Initializing modem..."));
  modemSerial.begin(MODEM_BAUD, SERIAL_8N1, MODEM_RX, MODEM_TX);
  delay(3000); // Wait for modem to power on

  modem.init();

  Serial.println(F("? Waiting for modem to be ready..."));
  while (!modem.testAT(5000)) {
    Serial.print(F("."));
    delay(1000);
  }
  Serial.println(F("\n? Modem is ready."));
}

/**
 * @brief Prints detailed information about the modem and network status.
 */
void printModemStatus() {
  Serial.println(F("--- Modem Status ---"));
  String imei = modem.getIMEI();
  Serial.println("IMEI: " + imei);

  int csq = modem.getSignalQuality();
  Serial.println("Signal Quality: " + String(csq));

  int simStatus = modem.getSimStatus();
  Serial.println("SIM Status: " + String(simStatus));

  String ccid = modem.getSimCCID();
  Serial.println("CCID: " + ccid);

  String oper = modem.getOperator();
  Serial.println("Operator: " + oper);

  Serial.println(F("--------------------"));
}

/**
 * @brief Waits for the modem to register on the cellular network.
 * @return True if registered, false otherwise.
 */
bool waitForNetwork() {
  Serial.println(F("? Waiting for network registration..."));
  unsigned long start = millis();
  while (millis() - start < 60000L) {
    int regStatus = modem.getRegistrationStatus();
    Serial.println("Network registration status: " + String(regStatus));
    if (regStatus == 1 || regStatus == 5) {
      Serial.println(F("? Registered on network."));
      return true;
    }
    delay(2000);
  }
  return false;
}

/**
 * @brief Manually establishes a GPRS connection using AT commands.
 * @return True if connected, false otherwise.
 */
bool manualGprsConnect() {
  Serial.println(F("? Connecting to GPRS..."));
  if (!sendATCommand(("AT+CGDCONT=1,\"IP\",\"" + String(apn) + "\"").c_str(), 5000, "OK")) {
    Serial.println(F("? Failed to set APN."));
    return false;
  }
  if (!sendATCommand("AT+CGACT=1,1", 20000, "OK")) {
    Serial.println(F("? Failed to activate GPRS context."));
    return false;
  }
  String ip_addr;
  if (!sendATCommand("AT+CGPADDR=1", 10000, "+CGPADDR", &ip_addr)) {
    Serial.println(F("? Failed to get IP address."));
    return false;
  }
  
  // Clean up the IP address string
  int commaIndex = ip_addr.indexOf(',');
  if (commaIndex != -1) {
    ip_addr = ip_addr.substring(commaIndex + 1);
  }
  ip_addr.trim();
  
  Serial.println("? GPRS Connected. IP: " + ip_addr);
  return true;
}

/**
 * @brief Opens an HTTPS session with the server.
 * @return True on success, false otherwise.
 */
bool openHttpsSession() {
  if (!sendATCommand("AT+CHTTPSSTART", 20000, "OK")) {
    Serial.println(F("? Failed to start HTTPS service."));
    return false;
  }

  // This command can take a long time. We must wait for the +CHTTPSOPSE URC.
  if (!sendATCommand(("AT+CHTTPSOPSE=\"" + String(server) + "\"," + String(port)).c_str(), 30000, "+CHTTPSOPSE: 0")) {
      Serial.println(F("? Failed to open HTTPS session with server."));
      closeHttpsSession();
      return false;
  }
  
  if (!sendATCommand(("AT+CHTTPSPARA=\"URL\",\"" + String(endpoint) + "\"").c_str(), 20000, "OK")) {
    Serial.println(F("? Failed to set URL parameter."));
    closeHttpsSession();
    return false;
  }

  return true;
}

/**
 * @brief Sets all necessary HTTP headers for a chunk upload.
 * @return True on success, false otherwise.
 */
bool setRequestHeaders(const char* filename, size_t offset, size_t chunkSize, size_t totalSize) {
    String headers;

    headers = "Content-Type: application/octet-stream";
    if (!sendATCommand(("AT+CHTTPSPARA=\"USERDATA\",\"" + headers + "\"").c_str(), 5000, "OK")) return false;
    
    headers = "X-Filename: " + String(filename).substring(String(filename).lastIndexOf('/') + 1);
    if (!sendATCommand(("AT+CHTTPSPARA=\"USERDATA\",\"" + headers + "\"").c_str(), 5000, "OK")) return false;

    headers = "X-Chunk-Offset: " + String(offset);
    if (!sendATCommand(("AT+CHTTPSPARA=\"USERDATA\",\"" + headers + "\"").c_str(), 5000, "OK")) return false;
    
    headers = "X-Chunk-Size: " + String(chunkSize);
    if (!sendATCommand(("AT+CHTTPSPARA=\"USERDATA\",\"" + headers + "\"").c_str(), 5000, "OK")) return false;
    
    headers = "X-Total-Size: " + String(totalSize);
    if (!sendATCommand(("AT+CHTTPSPARA=\"USERDATA\",\"" + headers + "\"").c_str(), 5000, "OK")) return false;

    return true;
}

/**
 * @brief Closes the currently open HTTPS session.
 */
void closeHttpsSession() {
  sendATCommand("AT+CHTTPSCLSE", 10000, "OK");
  sendATCommand("AT+CHTTPSSTOP", 10000, "OK");
}

/**
 * @brief Uploads a file from the SD card in chunks.
 * @param filename The full path of the file to upload.
 */
void uploadFile(const char* filename) {
  File file = SD.open(filename);
  if (!file) {
    Serial.println("? Failed to open file: " + String(filename));
    return;
  }

  size_t totalSize = file.size();
  size_t offset = 0;
  int retryCount = 0;
  const int maxRetries = 3;

  while (offset < totalSize) {
    if (retryCount >= maxRetries) {
      Serial.println("? Failed to upload chunk at offset " + String(offset) + " after " + String(maxRetries) + " retries. Aborting.");
      break;
    }

    Serial.println("  Attempt " + String(retryCount + 1) + "/" + String(maxRetries) + " to send chunk at offset " + String(offset) + "...");

    if (!sendChunk(file, offset, totalSize)) {
      Serial.println("    ...chunk upload failed.");
      retryCount++;
      delay(5000); // Wait before retrying
    } else {
      Serial.println("    ...chunk uploaded successfully.");
      offset += CHUNK_SIZE;
      retryCount = 0; // Reset retry count on success
    }
  }

  file.close();
  if (offset >= totalSize) {
    Serial.println(F("? File upload completed successfully."));
  } else {
    Serial.println(F("? File upload failed."));
  }
}

/**
 * @brief Sends a single chunk of a file.
 * @param file The file object.
 * @param offset The starting position of the chunk.
 * @param totalSize The total size of the file.
 * @return True on success, false otherwise.
 */
bool sendChunk(File& file, size_t offset, size_t totalSize) {
  size_t chunkSize = min((size_t)CHUNK_SIZE, totalSize - offset);

  if (!openHttpsSession()) {
    return false;
  }

  if (!setRequestHeaders(file.name(), offset, chunkSize, totalSize)) {
      Serial.println(F("? Failed to set headers."));
      closeHttpsSession();
      return false;
  }

  // Set the size of the data to be sent
  if (!sendATCommand(("AT+CHTTPSD=" + String(chunkSize)).c_str(), 10000, "DOWNLOAD")) {
    Serial.println(F("? Failed to set data size."));
    closeHttpsSession();
    return false;
  }

  // Read chunk from SD card and write to modem
  uint8_t buffer[256];
  file.seek(offset);
  size_t sentBytes = 0;
  while(sentBytes < chunkSize) {
    size_t toRead = min((size_t)sizeof(buffer), chunkSize - sentBytes);
    size_t readBytes = file.read(buffer, toRead);
    modemSerial.write(buffer, readBytes);
    sentBytes += readBytes;
  }

  // Wait for the send confirmation
  String response;
  if (!sendATCommand(nullptr, 30000, "+CHTTPS: 0", &response)) {
    Serial.println("? Chunk upload failed. Server response:");
    Serial.println(response);
    closeHttpsSession();
    return false;
  }

  closeHttpsSession();
  return true;
}

// =================================================================
//
// UTILITY FUNCTIONS
//
// =================================================================

/**
 * @brief Sends an AT command and waits for a specific response.
 * @param cmd The command to send. If null, just reads from the stream.
 * @param timeout The time to wait for the response.
 * @param expectedResponse The string to look for in the response.
 * @param responseBuffer A buffer to store the modem's full response.
 * @return True if the expected response is found, false otherwise.
 */
bool sendATCommand(const char* cmd, unsigned long timeout, const char* expectedResponse, String* responseBuffer = nullptr) {
    String res;
    if (cmd) {
        Serial.println("[AT SEND] " + String(cmd));
        modemSerial.println(cmd);
    }
    
    unsigned long start = millis();
    while (millis() - start < timeout) {
        while (modemSerial.available()) {
            char c = modemSerial.read();
            res += c;
        }
        if (res.indexOf(expectedResponse) != -1) {
            if (responseBuffer) *responseBuffer = res;
            Serial.println("[AT RECV] " + res);
            return true;
        }
    }
    
    Serial.println("[AT RECV TIMEOUT] " + res);
    if (responseBuffer) *responseBuffer = res;
    return false;
}

/**
 * @brief Gets the size of a file on the SD card.
 * @param filename The name of the file.
 * @return The size in bytes, or 0 if the file doesn't exist.
 */
size_t getFileSize(const char* filename) {
  File file = SD.open(filename);
  if (!file) {
    return 0;
  }
  size_t size = file.size();
  file.close();
  return size;
}

    