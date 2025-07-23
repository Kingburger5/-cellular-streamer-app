// This defines the modem type for the TinyGSM library.
#define TINY_GSM_MODEM_SIM7600
#define TINY_GSM_DEBUG Serial // Route TinyGSM debug prints to the main serial port

#include <HardwareSerial.h>
#include <TinyGsmClient.h>
#include <SD.h>
#include <SPI.h>

// Pin definitions for the modem and SD card
#define MODEM_TX 17
#define MODEM_RX 16
#define MODEM_BAUD 115200
#define SD_CS 5

// File upload settings
#define CHUNK_SIZE 4096 // Use a larger chunk size for faster uploads

// Server details
const char* server = "6000-firebase-studio-1753223410587.cluster-73qgvk7hjjadkrjeyexca5ivva.cloudworkstations.dev";
const char* endpoint = "/api/upload";
const int port = 443;

// APN for the One NZ network
const char* apn = "internet";

// Hardware serial for communication with the modem
HardwareSerial modemSerial(1);

// Global objects for the modem and SD card file
TinyGsm modem(modemSerial);
File uploadFile;


// --- Function Prototypes ---
void printModemStatus();
bool setupModem();
bool waitForNetwork();
bool manualGprsConnect();
void sendFileChunks(const char* filename);
bool openHttpsSession(const char* filename, size_t offset, size_t chunkSize, size_t totalSize);
bool sendChunk(const uint8_t* buffer, size_t size);
bool closeHttpsSession();
bool sendATCommand(const char* cmd, unsigned long timeout, const char* expected_response);
bool sendATCommand(const __FlashStringHelper* cmd, unsigned long timeout, const char* expected_response);


void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println(F("? Booting..."));

    // Initialize SD card
    if (!SD.begin(SD_CS)) {
        Serial.println(F("? SD card initialization failed. Halting."));
        while (true);
    }
    Serial.println(F("? SD card ready."));

    // Initialize modem
    if (!setupModem()) {
        Serial.println(F("? Modem initialization failed. Halting."));
        while (true);
    }

    // Print detailed modem status
    printModemStatus();

    // Wait for network registration
    if (!waitForNetwork()) {
        Serial.println(F("? Failed to register on network. Halting."));
        while (true);
    }
    Serial.println(F("? Registered on network."));

    // Connect to GPRS
    if (!manualGprsConnect()) {
        Serial.println(F("? GPRS connection failed. Halting."));
        while (true);
    }

    // Begin the file upload process
    sendFileChunks("/sigma2.wav");

    Serial.println(F("? Task finished. Entering idle loop."));
}

void loop() {
    // The main logic is in setup(), so loop is empty.
    delay(10000);
}

/**
 * Initializes the modem and ensures it's ready for commands.
 */
bool setupModem() {
    Serial.println(F("? Initializing modem..."));
    modemSerial.begin(MODEM_BAUD, SERIAL_8N1, MODEM_RX, MODEM_TX);
    delay(1000); // Wait a moment for serial to settle

    Serial.println(F("? Waiting for modem to be ready..."));
    unsigned long start = millis();
    while (millis() - start < 30000) { // 30-second timeout
        if (modem.testAT(1000)) {
            Serial.println(F("? Modem is ready."));
            return true;
        }
        Serial.print(".");
        delay(1000);
    }
    return false;
}

/**
 * Prints detailed status information from the modem.
 */
void printModemStatus() {
    Serial.println(F("--- Modem Status ---"));
    String imei = modem.getIMEI();
    if (imei.length() > 0) {
        Serial.println("IMEI: " + imei);
    }

    int csq = modem.getSignalQuality();
    Serial.println("Signal Quality: " + String(csq));

    int simStatus = modem.getSimStatus();
    Serial.println("SIM Status: " + String(simStatus));

    String ccid = modem.getSimCCID();
    if (ccid.length() > 0) {
        Serial.println("CCID: " + ccid);
    }

    String op = modem.getOperator();
    if (op.length() > 0) {
        Serial.println("Operator: " + op);
    }
    Serial.println(F("--------------------"));
}

/**
 * Waits for the modem to register on the cellular network.
 */
bool waitForNetwork() {
    Serial.println(F("? Waiting for network registration..."));
    unsigned long start = millis();
    while (millis() - start < 60000) { // 60-second timeout
        RegStatus status = modem.getRegistrationStatus();
        Serial.println("Network registration status: " + String(status));
        if (status == REG_OK_HOME || status == REG_OK_ROAMING) {
            return true;
        }
        delay(2000);
    }
    return false;
}

/**
 * Manually establishes a GPRS connection using AT commands.
 */
bool manualGprsConnect() {
    Serial.println(F("? Connecting to GPRS..."));

    // Set the APN
    if (!sendATCommand(("AT+CGDCONT=1,\"IP\",\"" + String(apn) + "\"").c_str(), 5000, "OK")) {
        Serial.println(F("? Failed to set APN."));
        return false;
    }

    // Activate the GPRS context
    if (!sendATCommand(F("AT+CGACT=1,1"), 30000, "OK")) {
        Serial.println(F("? Failed to activate GPRS context."));
        return false;
    }

    // Verify connection by getting IP address
    modemSerial.println(F("AT+CGPADDR=1"));
    String response = "";
    unsigned long start = millis();
    while (millis() - start < 10000) {
        if (modemSerial.available()) {
            response += modemSerial.readString();
        }
        if (response.indexOf("+CGPADDR:") != -1 && response.indexOf("OK") != -1) {
            int startIndex = response.indexOf(":") + 1;
            int endIndex = response.indexOf("\n", startIndex);
            String ip = response.substring(startIndex, endIndex);
            ip.trim();
            Serial.println("? GPRS Connected. IP: " + ip);
            return true;
        }
    }
    Serial.println(F("? Failed to get IP address."));
    return false;
}


/**
 * Manages the chunked file upload process.
 */
void sendFileChunks(const char* filename) {
    uploadFile = SD.open(filename);
    if (!uploadFile) {
        Serial.println(F("? Failed to open file for upload."));
        return;
    }

    size_t totalSize = uploadFile.size();
    size_t offset = 0;
    
    Serial.printf("? Preparing to upload %s (%d bytes)\n", filename, totalSize);

    while (offset < totalSize) {
        size_t chunkSize = min((size_t)CHUNK_SIZE, totalSize - offset);
        uint8_t buffer[chunkSize];
        uploadFile.seek(offset);
        uploadFile.read(buffer, chunkSize);

        bool success = false;
        for (int retry = 0; retry < 3; retry++) {
            Serial.printf("  Attempt %d/3 to send chunk at offset %d...\n", retry + 1, offset);
            
            // Open HTTPS session with all headers
            if (openHttpsSession(filename, offset, chunkSize, totalSize)) {
                // Send the actual chunk data
                if (sendChunk(buffer, chunkSize)) {
                    success = true;
                    break; // Success, exit retry loop
                }
            }
            // Always ensure the session is closed on failure before retrying
            closeHttpsSession();
            delay(2000); // Wait before retrying
        }

        if (success) {
            offset += chunkSize;
        } else {
            Serial.printf("? Failed to upload chunk at offset %d after 3 retries. Aborting.\n", offset);
            break;
        }
    }

    uploadFile.close();
    Serial.println(F("? Finished upload process."));
}

/**
 * Starts an HTTPS session and sets all required headers.
 */
bool openHttpsSession(const char* filename, size_t offset, size_t chunkSize, size_t totalSize) {
    if (!sendATCommand(F("AT+CHTTPSSTART"), 5000, "OK")) return false;
    
    String cmd = "AT+CHTTPSOPSE=\"" + String(server) + "\"," + String(port);
    if (!sendATCommand(cmd.c_str(), 30000, "OK")) return false;

    if (!sendATCommand(("AT+CHTTPSPARA=\"URL\",\"" + String(endpoint) + "\"").c_str(), 5000, "OK")) {
        Serial.println(F("? Failed to set URL parameter."));
        return false;
    }

    // Set headers one by one for reliability
    String headerCmd;
    headerCmd = "AT+CHTTPSPARA=\"USERDATA\",\"Content-Type: application/octet-stream\"";
    if (!sendATCommand(headerCmd.c_str(), 5000, "OK")) { Serial.println(F("? Failed to set Content-Type.")); return false; }
    
    headerCmd = "AT+CHTTPSPARA=\"USERDATA\",\"X-Filename: " + String(filename) + "\"";
    if (!sendATCommand(headerCmd.c_str(), 5000, "OK")) { Serial.println(F("? Failed to set X-Filename.")); return false; }

    headerCmd = "AT+CHTTPSPARA=\"USERDATA\",\"X-Chunk-Offset: " + String(offset) + "\"";
    if (!sendATCommand(headerCmd.c_str(), 5000, "OK")) { Serial.println(F("? Failed to set X-Chunk-Offset.")); return false; }

    headerCmd = "AT+CHTTPSPARA=\"USERDATA\",\"X-Chunk-Size: " + String(chunkSize) + "\"";
    if (!sendATCommand(headerCmd.c_str(), 5000, "OK")) { Serial.println(F("? Failed to set X-Chunk-Size.")); return false; }

    headerCmd = "AT+CHTTPSPARA=\"USERDATA\",\"X-Total-Size: " + String(totalSize) + "\"";
    if (!sendATCommand(headerCmd.c_str(), 5000, "OK")) { Serial.println(F("? Failed to set X-Total-Size.")); return false; }

    headerCmd = "AT+CHTTPSPARA=\"USERDATA\",\"Content-Length: " + String(chunkSize) + "\"";
    if (!sendATCommand(headerCmd.c_str(), 5000, "OK")) { Serial.println(F("? Failed to set Content-Length.")); return false; }

    return true;
}

/**
 * Sends the binary data chunk and verifies the server response.
 */
bool sendChunk(const uint8_t* buffer, size_t size) {
    if (!sendATCommand(("AT+CHTTPSSEND=" + String(size)).c_str(), 10000, ">")) {
        Serial.println(F("? Failed to get prompt for data send."));
        return false;
    }

    // Send the binary data
    Serial.printf("? Uploading chunk data (%d bytes)...\n", size);
    modemSerial.write(buffer, size);
    
    // Wait for the server response after sending data
    String response = "";
    unsigned long start = millis();
    while (millis() - start < 30000) { // 30-second timeout for server response
        if (modemSerial.available()) {
            char c = modemSerial.read();
            response += c;
             // Check for "200 OK" which indicates success from the server
            if (response.indexOf("200 OK") != -1) {
                Serial.println(F("? Chunk uploaded successfully."));
                return true;
            }
             // Check for SEND OK which is from modem, not server
            if (response.indexOf("+CHTTPSNOTIF: PEER RST") != -1 || response.indexOf("ERROR") != -1) {
                 break;
            }
        }
    }

    Serial.println(F("? Chunk upload failed. Server response:"));
    Serial.println(response);
    return false;
}

/**
 * Closes the HTTPS session and stops the service.
 */
bool closeHttpsSession() {
    sendATCommand(F("AT+CHTTPSCLSE"), 5000, "OK");
    return sendATCommand(F("AT+CHTTPSSTOP"), 5000, "OK");
}


/**
 * Helper function to send an AT command and wait for an expected response.
 */
bool sendATCommand(const char* cmd, unsigned long timeout, const char* expected_response) {
    String response = "";
    modemSerial.println(cmd);
    Serial.print(F("[AT SEND] "));
    Serial.println(cmd);

    unsigned long start = millis();
    while (millis() - start < timeout) {
        while (modemSerial.available()) {
            char c = modemSerial.read();
            response += c;
        }
        if (response.indexOf(expected_response) != -1) {
            Serial.print(F("[AT RECV] "));
            Serial.println(response);
            return true;
        }
    }
    Serial.print(F("[AT RECV TIMEOUT] "));
    Serial.println(response);
    return false;
}

bool sendATCommand(const __FlashStringHelper* cmd, unsigned long timeout, const char* expected_response) {
    return sendATCommand((const char*)cmd, timeout, expected_response);
}