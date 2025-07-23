#define TINY_GSM_MODEM_SIM7600  // Define modem type

#include <HardwareSerial.h>
#include <TinyGsmClient.h>
#include <SD.h>
#include <SPI.h>

// Your board's pin definitions
#define MODEM_RX 16
#define MODEM_TX 17
#define MODEM_BAUD 115200
#define SD_CS 5

// File upload settings
#define CHUNK_SIZE 4096 // Increased for better performance

HardwareSerial modemSerial(1);
TinyGsm modem(modemSerial);

// --- Server Configuration ---
const char server[] = "6000-firebase-studio-1753223410587.cluster-73qgvk7hjjadkrjeyexca5ivva.cloudworkstations.dev";
const int port = 443;
const char endpoint[] = "/api/upload";
const char apn[] = "internet"; // APN for One NZ
const char gprsUser[] = "";
const char gprsPass[] = "";

// --- AT Command Helpers ---
String sendATCommand(const char* cmd, unsigned long timeout) {
  modemSerial.println(cmd);
  String response = "";
  unsigned long startTime = millis();
  while (millis() - startTime < timeout) {
    if (modemSerial.available()) {
      response += (char)modemSerial.read();
    }
  }
  Serial.print("[RAW RSP] "); Serial.println(response);
  return response;
}

bool sendATCommandCheck(const char* cmd, unsigned long timeout, const char* okResp, const char* errResp = "ERROR") {
    modemSerial.println(cmd);
    String response;
    if (modem.waitResponse(timeout, response) != 1) {
        Serial.print("Timeout on: "); Serial.println(cmd);
        return false;
    }
    return response.indexOf(okResp) != -1;
}


// --- Core Functions ---
bool setupModem() {
  Serial.println("Initializing modem...");
  modemSerial.begin(MODEM_BAUD, SERIAL_8N1, MODEM_RX, MODEM_TX);
  
  delay(1000); // Wait for serial
  
  modem.init();

  Serial.println("Waiting for modem to be ready...");
  if (!modem.testAT(5000)) {
    Serial.println("Modem not responding.");
    return false;
  }
  
  Serial.println("Modem is ready.");
  return true;
}

bool waitForNetwork() {
  Serial.println("Waiting for network registration...");
  unsigned long start = millis();
  while (millis() - start < 60000L) { // 60-second timeout
    int regStatus = modem.getRegistrationStatus();
    Serial.print("Network registration status: "); Serial.println(regStatus);
    if (regStatus == 1 || regStatus == 5) { // Registered home or roaming
      Serial.println("Registered on network.");
      return true;
    }
    delay(1000);
  }
  Serial.println("Failed to register on network.");
  return false;
}

bool gprsConnect() {
  Serial.println("Connecting to GPRS...");
  if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
    Serial.println("GPRS connection failed.");
    return false;
  }

  if (modem.isGprsConnected()) {
    Serial.println("GPRS connected.");
    Serial.print("Local IP: "); Serial.println(modem.getLocalIP());
    return true;
  }
  
  Serial.println("GPRS connection status unknown.");
  return false;
}

bool openHttpsSession() {
  if (!sendATCommandCheck("AT+CHTTPSSTART", 10000, "OK")) {
    // If it fails, maybe it's already open. Try to stop and restart.
    sendATCommand("AT+CHTTPSSTOP", 5000);
    if (!sendATCommandCheck("AT+CHTTPSSTART", 10000, "OK")) {
        Serial.println("Failed to start HTTPS service.");
        return false;
    }
  }
  
  String cmd = "AT+CHTTPSOPSE=\"" + String(server) + "\"," + String(port);
  if (!sendATCommandCheck(cmd.c_str(), 20000, "+CHTTPSOPSE: 0")) {
    Serial.println("Failed to open HTTPS session.");
    sendATCommand("AT+CHTTPSCLSE", 5000); // Clean up on failure
    return false;
  }

  Serial.println("HTTPS session opened.");
  return true;
}

void closeHttpsSession() {
    Serial.println("Closing HTTPS session...");
    sendATCommand("AT+CHTTPSCLSE", 5000);
    sendATCommand("AT+CHTTPSSTOP", 5000);
}

bool sendChunk(const uint8_t* buffer, size_t chunkSize, const char* filename, size_t offset, size_t totalSize) {
    String headers = "POST " + String(endpoint) + " HTTP/1.1\r\n" +
                     "Host: " + String(server) + "\r\n" +
                     "X-Filename: " + String(filename).substring(String(filename).lastIndexOf('/') + 1) + "\r\n" +
                     "X-Chunk-Offset: " + String(offset) + "\r\n" +
                     "X-Chunk-Size: " + String(chunkSize) + "\r\n" +
                     "X-Total-Size: " + String(totalSize) + "\r\n" +
                     "Content-Type: application/octet-stream\r\n" +
                     "Content-Length: " + String(chunkSize) + "\r\n" +
                     "Connection: keep-alive\r\n\r\n";

    String cmd = "AT+CHTTPSSEND=" + String(headers.length());
    if (!sendATCommandCheck(cmd.c_str(), 5000, ">")) {
        Serial.println("Failed to prepare send command.");
        return false;
    }

    modemSerial.print(headers); // Send headers
    modemSerial.write(buffer, chunkSize); // Send binary data
    modemSerial.flush();
    
    // Wait for the final OK and the server response (+CHTTPSRECV)
    String response;
    if (modem.waitResponse(20000, response) != 1 || response.indexOf("OK") == -1) {
       Serial.println("Failed to get response after sending data.");
       return false;
    }

    if (response.indexOf("+CHTTPSRECV: DATA") != -1) {
        if (response.indexOf("HTTP/1.1 200") != -1 || response.indexOf("HTTP/1.1 201") != -1) {
            Serial.println("Chunk uploaded successfully.");
            return true;
        } else {
            Serial.println("Server returned non-200/201 status.");
            Serial.println(response);
            return false;
        }
    }
    
    Serial.println("Unexpected response format after sending chunk.");
    return false;
}


void uploadFile(const char* filename) {
    File file = SD.open(filename);
    if (!file) {
        Serial.println("Failed to open file for upload.");
        return;
    }

    size_t totalSize = file.size();
    if (totalSize == 0) {
        Serial.println("File is empty, skipping upload.");
        file.close();
        return;
    }

    Serial.printf("Preparing to upload %s (%d bytes)\n", filename, totalSize);

    if (!openHttpsSession()) {
        file.close();
        return;
    }

    size_t offset = 0;
    while (offset < totalSize) {
        size_t chunkSize = min((size_t)CHUNK_SIZE, totalSize - offset);
        uint8_t buffer[chunkSize];

        file.seek(offset);
        file.read(buffer, chunkSize);
        
        Serial.printf("Uploading chunk: offset=%d, size=%d\n", offset, chunkSize);
        
        if (!sendChunk(buffer, chunkSize, filename, offset, totalSize)) {
            Serial.println("Chunk upload failed. Aborting.");
            break;
        }
        
        offset += chunkSize;
    }

    closeHttpsSession();
    file.close();
    Serial.println("Upload process finished.");
}


void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Booting...");

  if (!SD.begin(SD_CS)) {
    Serial.println("SD card initialization failed. Halting.");
    while (true);
  }
  Serial.println("SD card ready.");

  if (!setupModem()) {
    Serial.println("Modem setup failed. Halting.");
    while (true);
  }
  
  if (!waitForNetwork()) {
    Serial.println("Network connection failed. Halting.");
    while (true);
  }

  if (!gprsConnect()) {
    Serial.println("GPRS connection failed. Halting.");
    while (true);
  }

  uploadFile("/sigma2.wav");

  Serial.println("Task finished. Entering idle loop.");
}

void loop() {
  // Idle loop.
  delay(10000);
}
