
#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <HardwareSerial.h>

#define TINY_GSM_MODEM_SIM7600
#define TINY_GSM_DEBUG Serial
#define DUMP_AT_COMMANDS

#include <TinyGsmClient.h>

// Pin definitions
#define SD_CS_PIN 5
#define MODEM_RX_PIN 16
#define MODEM_TX_PIN 17
#define MODEM_BAUD_RATE 115200

// Server and APN configuration
const char *server = "6000-firebase-studio-1753223410587.cluster-73qgvk7hjjadkrjeyexca5ivva.cloudworkstations.dev";
const int port = 443;
const char *upload_path = "/api/upload";
const char apn[] = "internet";
const char gprsUser[] = "";
const char gprsPass[] = "";

// Serial for modem communication
HardwareSerial modemSerial(1);
TinyGsm modem(modemSerial);

// --- UTILITY FUNCTIONS ---

bool sendATCommand(const char* cmd, unsigned long timeout = 10000, const char* expected_response = "OK") {
    #ifdef DUMP_AT_COMMANDS
        Serial.print("[AT SEND] ");
        Serial.println(cmd);
    #endif
    modemSerial.println(cmd);
    String response;
    if (modem.waitResponse(timeout, response) != 1) {
        #ifdef DUMP_AT_COMMANDS
            Serial.print("[DEBUG] Timeout waiting for: ");
            Serial.println(expected_response);
            Serial.print("[DEBUG] Received: ");
            Serial.println(response);
        #endif
        return false;
    }
    #ifdef DUMP_AT_COMMANDS
        Serial.print("[AT RECV] ");
        Serial.println(response);
    #endif
    return response.indexOf(expected_response) != -1;
}

bool sendATCommandCheck(const char* cmd, unsigned long timeout, const char* okResp, const char* errResp = nullptr) {
    #ifdef DUMP_AT_COMMANDS
        Serial.print("[AT SEND] ");
        Serial.println(cmd);
    #endif
    modemSerial.println(cmd);
    String response;
    if (modem.waitResponse(timeout, response) != 1) {
        #ifdef DUMP_AT_COMMANDS
            Serial.print("[DEBUG] Timeout waiting for response");
            Serial.print("[DEBUG] Received: ");
            Serial.println(response);
        #endif
        return false;
    }
    #ifdef DUMP_AT_COMMANDS
        Serial.print("[AT RECV] ");
        Serial.println(response);
    #endif
    if (response.indexOf(okResp) != -1) {
        return true;
    }
    if (errResp != nullptr && response.indexOf(errResp) != -1) {
        return false;
    }
    return false;
}

// --- CORE FUNCTIONS ---

void setupModem() {
    Serial.println("Initializing modem...");
    modemSerial.begin(MODEM_BAUD_RATE, SERIAL_8N1, MODEM_RX_PIN, MODEM_TX_PIN);
    
    // Restart if it's already on
    if (modem.testAT(1000)) {
        modem.restart();
    }

    int i = 10;
    while (i-- > 0) {
        if (modem.testAT(1000)) {
            Serial.println("✅ Modem restarted.");
            return;
        }
    }
    Serial.println("❌ Modem not responding.");
}

void printModemStatus() {
    Serial.println("--- Modem Status ---");
    String imei;
    modem.getIMEI(imei);
    Serial.println("IMEI: " + imei);
    Serial.println("Signal Quality: " + String(modem.getSignalQuality()));
    Serial.println("SIM Status: " + String(modem.getSimStatus()));
    String ccid;
    modem.getSimCCID(ccid);
    Serial.println("CCID: " + ccid);
    Serial.println("Operator: " + modem.getOperator());
    Serial.println("--------------------");
}


bool waitForNetwork() {
    Serial.println("Waiting for network registration...");
    int status;
    int retries = 30;
    while (retries-- > 0) {
        status = modem.getRegistrationStatus();
        Serial.println("Network registration status: " + String(status));
        if (status == 1 || status == 5) {
            Serial.println("✅ Registered on network.");
            return true;
        }
        delay(1000);
    }
    Serial.println("❌ Network registration failed.");
    return false;
}

bool openHttpsSession() {
    if (!sendATCommand("AT+CHTTPSSTART")) return false;
    
    String cmd = "AT+CHTTPSOPSE=\"" + String(server) + "\"," + String(port);
    if (!sendATCommandCheck(cmd.c_str(), 60000, "+CHTTPSOPSE: 0", "+CHTTPSOPSE:")){
        Serial.println("❌ Failed to open HTTPS session with server.");
        return false;
    }
    
    return true;
}

bool closeHttpsSession() {
    if (!sendATCommand("AT+CHTTPSCLSE")) return false;
    if (!sendATCommand("AT+CHTTPSSTOP")) return false;
    return true;
}

bool sendChunk(fs::File& file, const char* filename, size_t fileSize, size_t offset) {
    size_t chunkSize = file.size() - offset;
    if (chunkSize > 4096) {
        chunkSize = 4096;
    }

    uint8_t chunkBuffer[chunkSize];
    size_t bytesRead = file.read(chunkBuffer, chunkSize);
    if (bytesRead != chunkSize) {
        Serial.println("❌ File read error.");
        return false;
    }

    String headers = "x-filename: " + String(filename) + "\r\n" +
                     "x-chunk-offset: " + String(offset) + "\r\n" +
                     "x-chunk-size: " + String(chunkSize) + "\r\n" +
                     "x-total-size: " + String(fileSize) + "\r\n" +
                     "Content-Type: application/octet-stream";

    String postCmd = "AT+CHTTPSPOST=\"" + String(upload_path) + "\"," + String(headers.length() + 2) + "," + String(chunkSize) + ",15000";
    if (!sendATCommand(postCmd.c_str(), 2000, ">")) {
        Serial.println("❌ Modem did not respond to POST command. Aborting.");
        return false;
    }

    modemSerial.println(headers);
    modemSerial.write(chunkBuffer, chunkSize);
    modemSerial.println();

    String response;
    if (modem.waitResponse(30000, response) != 1 || response.indexOf("+CHTTPS: POST,20") == -1) {
        Serial.println("❌ Chunk upload failed. Response: " + response);
        return false;
    }
    
    Serial.println("✅ Chunk at offset " + String(offset) + " uploaded.");
    return true;
}


void sendFileInChunks(const char* filePath) {
    File file = SD.open(filePath, FILE_READ);
    if (!file) {
        Serial.println("❌ Failed to open file for reading: " + String(filePath));
        return;
    }

    size_t fileSize = file.size();
    Serial.println("📤 Preparing to upload " + String(filePath) + " (" + String(fileSize) + " bytes)");

    size_t offset = 0;
    bool success = true;

    if (!openHttpsSession()) {
        file.close();
        return;
    }

    while (offset < fileSize) {
        if (!sendChunk(file, file.name(), fileSize, offset)) {
            success = false;
            break;
        }
        offset += 4096; // This should match the chunk size used in sendChunk
        if (offset > fileSize) offset = fileSize;
    }
    
    closeHttpsSession();

    if (success) {
        Serial.println("✅ File upload complete.");
    } else {
        Serial.println("❌ File upload failed.");
    }
    file.close();
}


// --- MAIN SETUP AND LOOP ---

void setup() {
    Serial.begin(115200);
    Serial.println("\n🔌 Booting...");

    if (!SD.begin(SD_CS_PIN)) {
        Serial.println("❌ SD card mount failed.");
        while (1);
    }
    Serial.println("✅ SD card ready.");

    setupModem();
    
    printModemStatus();

    Serial.println("📡 Connecting to network...");
    if (!waitForNetwork()) return;

    if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
        Serial.println("❌ GPRS connection failed.");
        return;
    }
    Serial.println("✅ GPRS connected.");
    
    Serial.println("Local IP: " + modem.getLocalIP());

    sendFileInChunks("/sigma2.wav");
}

void loop() {
    // Keep the main loop clean, all logic is in setup for this one-shot task.
    delay(10000);
}
