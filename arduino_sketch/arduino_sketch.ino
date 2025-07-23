
#define TINY_GSM_MODEM_SIM7600
#define TINY_GSM_DEBUG Serial

#include <SPI.h>
#include <SD.h>
#include "FS.h"
#include <HardwareSerial.h>

// Define your modem's pins and settings
#define MODEM_RX 16
#define MODEM_TX 17
#define MODEM_BAUD 115200

// Define your SD card CS pin
#define SD_CS 5

// Define the serial port for the modem
HardwareSerial modemSerial(1);

// Server and APN details
const char* server = "6000-firebase-studio-1753223410587.cluster-73qgvk7hjjadkrjeyexca5ivva.cloudworkstations.dev";
const char* endpoint = "/api/upload";
const char* apn = "internet"; // Use "internet" for One NZ

// File upload settings
const char* filename = "/sigma2.wav";
const size_t CHUNK_SIZE = 1024 * 4; // 4KB chunks

// Forward declarations for helper functions
void printModemStatus();
bool setupModem();
bool manualGprsConnect();
bool sendFileInChunks(const char* filePath);
bool sendATCommand(const char* cmd, unsigned long timeout, const char* expected_response);
bool sendATCommand(const __FlashStringHelper* cmd, unsigned long timeout, const char* expected_response);
String getResponse(const char* cmd, unsigned long timeout);

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println(F("?? Booting..."));

    // Initialize SD Card
    if (!SD.begin(SD_CS)) {
        Serial.println(F("? SD card initialization failed. Halting."));
        while (true);
    }
    Serial.println(F("? SD card ready."));

    // Initialize Modem
    if (!setupModem()) {
        Serial.println(F("? Modem initialization failed. Halting."));
        while (true);
    }

    // Connect to GPRS
    if (!manualGprsConnect()) {
        Serial.println(F("? GPRS connection failed. Halting."));
        while (true);
    }
    
    // Upload the file
    sendFileInChunks(filename);

    Serial.println(F("? Task finished. Entering idle loop."));
}

void loop() {
    // Keep the device alive, but do nothing.
    delay(10000);
}

/**
 * Initializes the modem and prints its status.
 */
bool setupModem() {
    Serial.println(F("? Initializing modem..."));
    modemSerial.begin(MODEM_BAUD, SERIAL_8N1, MODEM_RX, MODEM_TX);
    
    // Wait for the modem to become responsive
    Serial.println(F("? Waiting for modem to be ready..."));
    unsigned long start_time = millis();
    while (millis() - start_time < 30000) { // 30-second timeout
        if (sendATCommand("AT", 1000, "OK")) {
            Serial.println(F("? Modem is ready."));
            printModemStatus();
            return true;
        }
        delay(500);
    }
    return false;
}

/**
 * Prints detailed information about the modem and SIM status.
 */
void printModemStatus() {
    Serial.println(F("--- Modem Status ---"));

    String imei = getResponse("AT+CIMI", 1000);
    imei.trim();
    Serial.println("IMEI: " + imei);

    String signal = getResponse("AT+CSQ", 1000);
    signal.trim();
    Serial.println("Signal Quality: " + signal);

    String simStatus = getResponse("AT+CPIN?", 1000);
    simStatus.trim();
    Serial.println("SIM Status: " + simStatus);

    String ccid = getResponse("AT+CCID", 1000);
    ccid.trim();
    ccid.replace("+CCID: ", "");
    Serial.println("CCID: " + ccid);

    String op = getResponse("AT+COPS?", 1000);
    op.trim();
    op.replace("+COPS: 0,0,\"", "");
    op.replace("\"", "");
    Serial.println("Operator: " + op);

    Serial.println(F("--------------------"));
}

/**
 * Connects to GPRS manually using AT commands.
 */
bool manualGprsConnect() {
    Serial.println(F("? Connecting to GPRS..."));

    // Ensure GPRS is not already active
    sendATCommand("AT+CGACT=0,1", 5000, "OK");
    
    // Set the APN
    String cmd = "AT+CGDCONT=1,\"IP\",\"" + String(apn) + "\"";
    if (!sendATCommand(cmd.c_str(), 5000, "OK")) {
        Serial.println(F("? Failed to set APN."));
        return false;
    }

    // Activate the GPRS context
    if (!sendATCommand("AT+CGACT=1,1", 30000, "OK")) {
        Serial.println(F("? Failed to activate GPRS context."));
        return false;
    }
    
    // Check for an IP address
    unsigned long start_time = millis();
    while (millis() - start_time < 30000) {
        String res = getResponse("AT+CGPADDR=1", 3000);
        if (res.indexOf("+CGPADDR: 1,") != -1) {
            res.trim();
            res.replace("+CGPADDR: 1,", "");
            Serial.println("? GPRS Connected. IP: " + res);
            return true;
        }
        delay(1000);
    }

    return false;
}

/**
 * Uploads a file from the SD card in chunks using HTTPS.
 */
bool sendFileInChunks(const char* filePath) {
    File file = SD.open(filePath);
    if (!file) {
        Serial.println("? Failed to open file: " + String(filePath));
        return false;
    }

    size_t totalSize = file.size();
    size_t offset = 0;
    
    Serial.printf("? Preparing to upload %s (%d bytes)\n", filePath, totalSize);

    while (offset < totalSize) {
        // 1. Start HTTPS session
        if (!sendATCommand("AT+CHTTPSSTART", 5000, "OK")) {
            Serial.println(F("? HTTPS service start failed. Aborting."));
            file.close();
            return false;
        }

        // 2. Open HTTPS session with the server
        String cmd = "AT+CHTTPSOPSE=\"" + String(server) + "\",443";
        if (!sendATCommand(cmd.c_str(), 30000, "+CHTTPSOPSE: 0")) {
            Serial.println(F("? Failed to open HTTPS session with server."));
            sendATCommand("AT+CHTTPSCLSE", 5000, "OK");
            sendATCommand("AT+CHTTPSSTOP", 5000, "OK");
            file.close();
            return false;
        }

        // 3. Prepare to send data
        size_t chunkSize = min((size_t)CHUNK_SIZE, totalSize - offset);
        String param_cmd = "AT+CHTTPSPARA=\"URL\",\"" + String(endpoint) + "\"";
        
        if (!sendATCommand(param_cmd.c_str(), 5000, "OK")) {
             Serial.println(F("? Failed to set URL parameter."));
             // Abort logic
        }

        String headers = "Content-Type: application/octet-stream\r\n";
        headers += "X-Filename: " + String(filePath).substring(String(filePath).lastIndexOf('/') + 1) + "\r\n";
        headers += "X-Chunk-Offset: " + String(offset) + "\r\n";
        headers += "X-Chunk-Size: " + String(chunkSize) + "\r\n";
        headers += "X-Total-Size: " + String(totalSize) + "\r\n";

        param_cmd = "AT+CHTTPSPARA=\"USERDATA\",\"" + headers + "\"";
        if (!sendATCommand(param_cmd.c_str(), 5000, "OK")) {
            Serial.println(F("? Failed to set headers."));
             // Abort logic
        }

        // 4. Send POST request and data
        if (!sendATCommand(F("AT+CHTTPSSEND=1"), 10000, ">")) {
             Serial.println(F("? Failed to get SEND prompt."));
             // Abort logic
        }

        uint8_t buffer[chunkSize];
        file.seek(offset);
        file.read(buffer, chunkSize);
        modemSerial.write(buffer, chunkSize);
        
        Serial.printf("? Uploading chunk at offset %d (%d bytes)...\n", offset, chunkSize);

        // Wait for the server response after sending data
        String response = "";
        unsigned long start_time = millis();
        while (millis() - start_time < 30000) {
            if (modemSerial.available()) {
                char c = modemSerial.read();
                response += c;
                if (response.endsWith("OK\r\n")) {
                    break;
                }
            }
        }
        
        // Check server response (crude check for now)
        if (response.indexOf("+CHTTPSNOTIF: 1,200") == -1) {
            Serial.println("? Chunk upload failed. Server response:");
            Serial.println(response);
            // Abort logic
        } else {
             Serial.println(F("? Chunk uploaded successfully."));
        }
        
        // 5. Close and stop the session
        sendATCommand("AT+CHTTPSCLSE", 5000, "OK");
        sendATCommand("AT+CHTTPSSTOP", 5000, "OK");
        
        offset += chunkSize;
        delay(500); // Small delay between chunks
    }

    file.close();
    Serial.println(F("? File upload finished."));
    return true;
}


// =================================================================
//                 LOWER-LEVEL HELPER FUNCTIONS
// =================================================================

/**
 * Sends an AT command and waits for a specific response.
 * @param cmd The AT command to send.
 * @param timeout The time to wait for the response.
 * @param expected_response The response string to look for.
 * @return True if the expected response is found, false otherwise.
 */
bool sendATCommand(const char* cmd, unsigned long timeout, const char* expected_response) {
    String res = getResponse(cmd, timeout);
    return res.indexOf(expected_response) != -1;
}

/**
 * Overloaded version of sendATCommand for FlashStringHelper.
 */
bool sendATCommand(const __FlashStringHelper* cmd, unsigned long timeout, const char* expected_response) {
    modemSerial.println(cmd);
    String res = "";
    unsigned long start_time = millis();
    while (millis() - start_time < timeout) {
        while (modemSerial.available()) {
            res += (char)modemSerial.read();
        }
        if (res.indexOf(expected_response) != -1) {
            return true;
        }
    }
    return false;
}

/**
 * Sends a command and returns the full response from the modem.
 * @param cmd The AT command to send.
 * @param timeout The maximum time to wait for a response.
 * @return The modem's response as a String.
 */
String getResponse(const char* cmd, unsigned long timeout) {
    String res = "";
    Serial.print(F("[AT SEND] "));
    Serial.println(cmd);
    
    modemSerial.println(cmd);

    unsigned long start_time = millis();
    while (millis() - start_time < timeout) {
        if (modemSerial.available()) {
            res += (char)modemSerial.read();
        }
    }
    
    Serial.print(F("[AT RECV] "));
    res.trim();
    Serial.println(res);
    return res;
}
