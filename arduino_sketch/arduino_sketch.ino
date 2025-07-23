
#define TINY_GSM_MODEM_SIM7600

#include <SD.h>
#include <SPI.h>
#include <TinyGsmClient.h>
#include <HardwareSerial.h>

// Pin definitions
#define SD_CS_PIN 5
#define MODEM_RX_PIN 16
#define MODEM_TX_PIN 17

// Server details
const char server[] = "6000-firebase-studio-1753223410587.cluster-73qgvk7hjjadkrjeyexca5ivva.cloudworkstations.dev";
const char resource[] = "/api/upload";
const int port = 443;

// APN details
const char apn[] = "internet";
const char gprsUser[] = "";
const char gprsPass[] = "";

// Buffer for reading file chunks
const size_t CHUNK_BUFFER_SIZE = 2048;
uint8_t chunkBuffer[CHUNK_BUFFER_SIZE];

// Hardware Serial for modem communication
HardwareSerial modemSerial(1);

// TinyGSM modem instance
TinyGsm modem(modemSerial);

// Function Prototypes
void setupModem();
bool waitForModemReady();
bool waitForNetwork();
void printModemStatus();
bool sendFileChunks(const char* filePath);
bool sendChunk(File& file, const char* filename, size_t fileSize, size_t offset);
bool openHttpsSession();
void closeHttpsSession();
bool sendATCommand(const char* cmd, unsigned long timeout, const char* expectedResponse);
bool sendATCommandCheck(const char* cmd, unsigned long timeout, const char* okResp, const char* errResp);

void setup() {
  Serial.begin(115200);
  delay(10);

  Serial.println(F("? Booting..."));

  // Initialize SD card
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println(F("? SD card initialization failed."));
    while (1);
  }
  Serial.println(F("? SD card ready."));

  // Initialize modem
  setupModem();
  
  if (!waitForModemReady()) {
      Serial.println(F("? Modem initialization failed. Halting."));
      while (true);
  }
  Serial.println(F("? Modem and SIM are ready."));

  printModemStatus();

  Serial.println(F("? Connecting to network..."));
  if (!waitForNetwork()) {
      Serial.println(F("? Network registration failed."));
      return;
  }
  Serial.println(F("? Registered on network."));
  
  // Connect to GPRS
  if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
    Serial.println(F("? GPRS connection failed."));
    return;
  }
  Serial.println(F("? GPRS connected."));
  String ip = modem.getLocalIP();
  Serial.print(F("Local IP: "));
  Serial.println(ip);
  
  // Start the file upload process
  const char* uploadFilePath = "/sigma2.wav"; // The file to upload
  Serial.print(F("? Preparing to upload "));
  Serial.print(uploadFilePath);
  File file = SD.open(uploadFilePath, FILE_READ);
  if (file) {
      Serial.print(" (");
      Serial.print(file.size());
      Serial.println(" bytes)");
      file.close();
      sendFileChunks(uploadFilePath);
  } else {
      Serial.println(" - file not found.");
  }

  Serial.println(F("? Task finished. Entering idle loop."));
}

void loop() {
  // The main logic is in setup() for this single-shot task.
  // The loop is kept empty.
  delay(10000); 
}

void setupModem() {
  modemSerial.begin(115200, SERIAL_8N1, MODEM_RX_PIN, MODEM_TX_PIN);
  Serial.println(F("Initializing modem..."));
  if (!modem.restart()) {
    Serial.println(F("? Modem restart failed."));
  }
  Serial.println(F("? Modem restarted."));
}

bool waitForModemReady() {
    Serial.println(F("Waiting for modem to be ready..."));
    for (int i = 0; i < 20; i++) {
        if (modem.testAT()) {
            SimStatus simStatus = modem.getSimStatus();
            if (simStatus == SIM_READY) {
                return true;
            }
        }
        delay(500);
    }
    return false;
}

bool waitForNetwork() {
    Serial.println(F("Waiting for network registration..."));
    for (int i = 0; i < 30; i++) { // Wait up to 30 seconds
        int regStatus = modem.getRegistrationStatus();
        Serial.print(F("Network registration status: "));
        Serial.println(regStatus);
        if (regStatus == 1 || regStatus == 5) { // Registered home or roaming
            return true;
        }
        delay(1000);
    }
    return false;
}

void printModemStatus() {
    Serial.println(F("--- Modem Status ---"));
    String imei = modem.getIMEI();
    Serial.print(F("IMEI: "));
    Serial.println(imei);

    int signalQuality = modem.getSignalQuality();
    Serial.print(F("Signal Quality: "));
    Serial.println(signalQuality);

    SimStatus simStatus = modem.getSimStatus();
    Serial.print(F("SIM Status: "));
    Serial.println(simStatus);

    String ccid = modem.getSimCCID();
    Serial.print(F("CCID: "));
    Serial.println(ccid);

    String operatorName = modem.getOperator();
    Serial.print(F("Operator: "));
    Serial.println(operatorName);
    Serial.println(F("--------------------"));
}

bool sendFileChunks(const char* filePath) {
  File file = SD.open(filePath, FILE_READ);
  if (!file) {
    Serial.println(F("? Failed to open file for reading."));
    return false;
  }

  size_t fileSize = file.size();
  size_t offset = 0;

  while (offset < fileSize) {
    bool success = sendChunk(file, file.name(), fileSize, offset);
    if (success) {
      size_t chunkSize = file.position() - offset;
      offset += chunkSize;
    } else {
      Serial.print(F("? Failed to upload chunk at offset "));
      Serial.print(offset);
      Serial.println(F(". Aborting."));
      file.close();
      return false;
    }
  }

  Serial.println(F("? File upload completed successfully."));
  file.close();
  return true;
}

bool sendChunk(File& file, const char* filename, size_t fileSize, size_t offset) {
  for (int attempt = 1; attempt <= 3; attempt++) {
    Serial.print(F("  Attempt "));
    Serial.print(attempt);
    Serial.print(F("/3 to send chunk at offset "));
    Serial.print(offset);
    Serial.println(F("..."));

    if (!openHttpsSession()) {
      Serial.println(F("    ...connection failed."));
      closeHttpsSession(); 
      delay(2000);
      continue;
    }

    size_t bytesRead = file.read(chunkBuffer, CHUNK_BUFFER_SIZE);
    if (bytesRead == 0) {
      Serial.println(F("? No bytes read from file."));
      closeHttpsSession();
      return false;
    }

    String headers;
    headers += "POST " + String(resource) + " HTTP/1.1\r\n";
    headers += "Host: " + String(server) + "\r\n";
    headers += "X-Filename: " + String(filename) + "\r\n";
    headers += "X-Chunk-Offset: " + String(offset) + "\r\n";
    headers += "X-Chunk-Size: " + String(bytesRead) + "\r\n";
    headers += "X-Total-Size: " + String(fileSize) + "\r\n";
    headers += "Content-Type: application/octet-stream\r\n";
    headers += "Content-Length: " + String(bytesRead) + "\r\n";
    headers += "Connection: close\r\n";
    headers += "\r\n";
    
    String fullCommand = "AT+CHTTPSSEND=" + String(headers.length() + bytesRead);
    if (!sendATCommand(fullCommand.c_str(), 10000, ">")) {
        Serial.println(F("? Failed on CHTTPSSEND command."));
        closeHttpsSession();
        delay(2000);
        continue;
    }
    
    modemSerial.print(headers);
    modemSerial.write(chunkBuffer, bytesRead);
    
    // Wait for the server response
    String response;
    if (modem.waitResponse(10000, response) != 1) {
        Serial.println(F("? No server response after sending chunk."));
        closeHttpsSession();
        delay(2000);
        continue;
    }

    Serial.print("Server Response: ");
    Serial.println(response);

    closeHttpsSession();

    if (response.indexOf("200 OK") != -1 || response.indexOf("201 Created") != -1) {
        Serial.println(F("    ...chunk sent successfully."));
        return true;
    } else {
        Serial.println(F("    ...chunk failed, unexpected server response."));
        delay(2000);
    }
  }
  return false;
}

bool openHttpsSession() {
  if (!sendATCommand("AT+CHTTPSSTART", 10000, "OK")) {
    return false;
  }
  
  String cmd = "AT+CHTTPSOPSE=\"" + String(server) + "\"," + String(port);
  if (!sendATCommand(cmd.c_str(), 20000, "+CHTTPSOPSE: 0")) {
    Serial.println(F("? Failed to open HTTPS session with server."));
    return false;
  }

  return true;
}

void closeHttpsSession() {
    sendATCommand("AT+CHTTPSCLSE", 10000, "OK");
    sendATCommand("AT+CHTTPSSTOP", 10000, "OK");
}

bool sendATCommand(const char* cmd, unsigned long timeout, const char* expectedResponse) {
    modem.sendAT(cmd);
    String response;
    if (modem.waitResponse(timeout, response) != 1) {
        Serial.print(F("[DEBUG] Timeout waiting for: "));
        Serial.println(expectedResponse);
        Serial.print(F("[DEBUG] Received: "));
        Serial.println(response);
        return false;
    }
    if (response.indexOf(expectedResponse) == -1) {
        Serial.print(F("[DEBUG] Unexpected response: "));
        Serial.println(response);
        return false;
    }
    return true;
}

bool sendATCommandCheck(const char* cmd, unsigned long timeout, const char* okResp, const char* errResp) {
    modem.sendAT(cmd);
    String response;
    if (modem.waitResponse(timeout, response) != 1) {
        Serial.println(F("[DEBUG] Command timeout"));
        return false;
    }
    if (response.indexOf(okResp) != -1) {
        return true;
    }
    if (response.indexOf(errResp) != -1) {
        return false;
    }
    return false;
}
