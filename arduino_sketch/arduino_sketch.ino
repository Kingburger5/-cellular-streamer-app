#include <SPI.h>
#include <SD.h>
#include <HardwareSerial.h>
#include <TinyGsmClient.h>

// Pin definitions
#define MODEM_RX 16
#define MODEM_TX 17
#define MODEM_BAUD 115200
#define SD_CS_PIN 5

// Modem and GPRS settings
const char apn[] = "internet"; // APN for your SIM card provider
const char gprsUser[] = "";
const char gprsPass[] = "";
const char server[] = "cellular-data-streamer.web.app";
const int port = 443;
const char* upload_path = "/api/upload";

// File upload settings
#define CHUNK_SIZE 4096

HardwareSerial modemSerial(1);
TinyGsm modem(modemSerial);
File uploadFile;

// --- Utility Functions ---

void printDebug(const char* message) {
    Serial.print("[DEBUG] ");
    Serial.println(message);
}

bool sendATCommand(const char* cmd, unsigned long timeout, const char* expected_response) {
    modemSerial.println(cmd);
    String response;
    if (modem.waitResponse(timeout, response) != 1) {
        printDebug("Timeout or no response.");
        return false;
    }
    return response.indexOf(expected_response) != -1;
}

bool sendATCommandCheck(const char* cmd, unsigned long timeout, const char* okResp = "OK", const char* errResp = "ERROR") {
    modemSerial.println(cmd);
    String response;
    if (modem.waitResponse(timeout, response) != 1) {
        String msg = "Timeout waiting for: ";
        msg += okResp;
        printDebug(msg.c_str());
        printDebug(response.c_str());
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

// --- Core Functions ---

bool waitForModemReady() {
  Serial.println("Waiting for modem to become ready...");
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
    Serial.println("Waiting for network registration...");
    int status;
    for (int i = 0; i < 20; i++) {
        status = modem.getRegistrationStatus();
        Serial.print("Network registration status: ");
        Serial.println(status);
        if (status == 1 || status == 5) { // Registered home or roaming
            Serial.println("✅ Registered on network.");
            return true;
        }
        delay(1000);
    }
    return false;
}

bool openHttpsSession() {
    if (!sendATCommandCheck("AT+CHTTPSSTART", 10000)) {
        return false;
    }

    String cmd = "AT+CHTTPSOPSE=\"";
    cmd += server;
    cmd += "\",";
    cmd += port;

    if (!sendATCommandCheck(cmd.c_str(), 20000, "+CHTTPSOPSE: 0")) {
        Serial.println("❌ Failed to open HTTPS session with server.");
        return false;
    }
    
    return true;
}

bool closeHttpsSession() {
    bool closed = sendATCommandCheck("AT+CHTTPSCLSE", 10000);
    bool stopped = sendATCommandCheck("AT+CHTTPSSTOP", 10000);
    return closed && stopped;
}

bool sendChunk(const char* filename, size_t totalSize, size_t offset, uint8_t* chunkBuffer, size_t chunkSize) {
    char headers[200];
    snprintf(headers, sizeof(headers),
             "x-filename: %s\r\nx-chunk-offset: %d\r\nx-chunk-size: %d\r\nx-total-size: %d",
             filename, offset, chunkSize, totalSize);
    
    int headersLength = strlen(headers);
    int totalPayloadLength = headersLength + 2 + chunkSize; // headers + \r\n + chunk

    char cmd[200];
    snprintf(cmd, sizeof(cmd), "AT+CHTTPSPOST=%s,%d,%d,%d", upload_path, headersLength, chunkSize, 10000);

    modemSerial.println(cmd);
    
    String response;
    if (modem.waitResponse(1000, response) != 1 || response.indexOf('>') == -1) {
        printDebug("Modem did not respond to POST command. Aborting.");
        printDebug(response.c_str());
        return false;
    }

    // Send headers then data
    modemSerial.print(headers);
    modemSerial.print("\r\n");
    modemSerial.write(chunkBuffer, chunkSize);
    modemSerial.flush();

    // Wait for final response
    if (modem.waitResponse(15000, response) != 1 || response.indexOf("+CHTTPS: POST,20") == -1) {
        printDebug("Failed to upload chunk or received non-2xx response.");
        printDebug(response.c_str());
        return false;
    }
    
    return true;
}

void uploadFileInChunks(const char* filename) {
    uploadFile = SD.open(filename, FILE_READ);
    if (!uploadFile) {
        Serial.println("❌ Failed to open file for reading");
        return;
    }

    size_t fileSize = uploadFile.size();
    Serial.print("📤 Preparing to upload ");
    Serial.print(filename);
    Serial.print(" (");
    Serial.print(fileSize);
    Serial.println(" bytes)");

    if (!openHttpsSession()) {
        uploadFile.close();
        closeHttpsSession();
        return;
    }

    size_t bytesSent = 0;
    while (bytesSent < fileSize) {
        uint8_t buffer[CHUNK_SIZE];
        size_t chunkSize = uploadFile.read(buffer, sizeof(buffer));
        if (chunkSize == 0) break;

        bool success = false;
        for (int i = 0; i < 3; i++) {
            if (sendChunk(uploadFile.name(), fileSize, bytesSent, buffer, chunkSize)) {
                success = true;
                break;
            }
            Serial.println("Retrying chunk...");
            delay(1000);
        }

        if (success) {
            bytesSent += chunkSize;
            Serial.print("Uploaded ");
            Serial.print(bytesSent);
            Serial.print("/");
            Serial.print(fileSize);
            Serial.println(" bytes");
        } else {
            Serial.println("❌ Failed to upload chunk after 3 retries. Aborting.");
            break; 
        }
    }
    
    uploadFile.close();
    closeHttpsSession();
    
    if (bytesSent == fileSize) {
        Serial.println("✅ File upload complete.");
    } else {
        Serial.println("❌ File upload failed.");
    }
}


// --- Setup and Loop ---

void setup() {
    Serial.begin(115200);
    while (!Serial);

    Serial.println("🔌 Booting...");

    if (!SD.begin(SD_CS_PIN)) {
        Serial.println("❌ SD card mount failed");
        while (1);
    }
    Serial.println("✅ SD card ready.");

    Serial.println("Initializing modem...");
    modemSerial.begin(MODEM_BAUD, SERIAL_8N1, MODEM_RX, MODEM_TX);
    
    if (!waitForModemReady()) {
        Serial.println("❌ Modem not responding.");
        while(1);
    }
    Serial.println("✅ Modem and SIM are ready.");

    // Print modem details
    Serial.println("--- Modem Status ---");
    Serial.print("IMEI: "); Serial.println(modem.getIMEI());
    Serial.print("Signal Quality: "); Serial.println(modem.getSignalQuality());
    Serial.print("SIM Status: "); Serial.println(modem.getSimStatus());
    Serial.print("CCID: "); Serial.println(modem.getSimCCID());
    Serial.print("Operator: "); Serial.println(modem.getOperator());
    Serial.println("--------------------");

    Serial.println("📡 Connecting to network...");
    if (!waitForNetwork()) {
        Serial.println("❌ Could not register on network.");
        while(1);
    }

    if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
        Serial.println("❌ GPRS connection failed.");
        while(1);
    }
    Serial.println("✅ GPRS connected.");

    Serial.print("Local IP: ");
    Serial.println(modem.getLocalIP());

    uploadFileInChunks("/sigma2.wav");
}

void loop() {
    // All work is done in setup for this example
}
