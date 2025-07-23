/**
 * @file arduino_sketch.ino
 * @brief This sketch connects to a cellular network, reads a WAV file from an SD card,
 *        and uploads it in chunks to a remote server using HTTPS POST requests.
 *        It is designed for an ESP32 with a SIM7600G-H modem.
 */

#define TINY_GSM_MODEM_SIM7600
#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <FS.h>
#include "utilities.h"

// TinyGSM library for cellular communication
#include <TinyGsmClient.h>

// Your board hardware definitions
#define MODEM_RX_PIN 16
#define MODEM_TX_PIN 17

// Server details for file upload
const char server[] = "6000-firebase-studio-1753223410587.cluster-73qgvk7hjjadkrjeyexca5ivva.cloudworkstations.dev";
const char resource[] = "/api/upload";
const int port = 443;

// Your GPRS credentials (leave empty if not needed)
const char apn[]  = "internet";
const char gprsUser[] = "";
const char gprsPass[] = "";

// Hardware serial for communication with the modem
HardwareSerial modemSerial(1);

// TinyGSM client for handling the cellular connection
TinyGsm modem(modemSerial);

// Chunk size for file upload
const size_t CHUNK_SIZE = 1024 * 4; // 4KB chunks

/**
 * @brief Sends an AT command and waits for a specific response.
 * @param cmd The AT command to send.
 * @param timeout Timeout in milliseconds to wait for the response.
 * @param okResp The expected successful response (e.g., "OK").
 * @param errResp An optional error response string.
 * @return True if the expected success response is received, false otherwise.
 */
bool sendATCommandCheck(const char* cmd, unsigned long timeout, const char* okResp, const char* errResp = nullptr) {
    modem.sendAT(cmd);
    String response;
    if (modem.waitResponse(timeout, response) != 1) {
        return false;
    }
    return response.indexOf(okResp) != -1;
}

/**
 * @brief Prints key status information about the modem.
 */
void printModemStatus() {
    Serial.println("--- Modem Status ---");
    
    String imei = modem.getIMEI();
    Serial.println("IMEI: " + imei);

    int csq = modem.getSignalQuality();
    Serial.println("Signal Quality: " + String(csq));

    SimStatus simStatus = modem.getSimStatus();
    Serial.println("SIM Status: " + String(simStatus));

    String ccid = modem.getSimCCID();
    Serial.println("CCID: " + ccid);

    String op = modem.getOperator();
    Serial.println("Operator: " + op);
    
    Serial.println("--------------------");
}


/**
 * @brief Sets up the modem, waiting for it to become responsive.
 * @return True if the modem is successfully initialized, false otherwise.
 */
bool setupModem() {
    modem.init();

    Serial.println("Waiting for modem to become responsive...");
    int retryCount = 0;
    while (!modem.testAT(1000)) {
        Serial.print(".");
        if (retryCount++ > 30) { // Wait for up to 30 seconds
            Serial.println("\n❌ Modem is not responsive.");
            return false;
        }
        delay(1000);
    }
    Serial.println("\n✅ Modem is responsive.");
    return true;
}


/**
 * @brief Waits for the modem to register on the cellular network.
 * @return True if registered successfully, false otherwise.
 */
bool waitForNetwork() {
    Serial.println("Waiting for network registration...");
    int retries = 0;
    while (retries < 30) {
        NetworkRegistrationState regState = modem.getRegistrationStatus();
        Serial.println("Network registration status: " + String(regState));
        if (regState == REG_OK_HOME || regState == REG_OK_ROAMING) {
            Serial.println("✅ Registered on network.");
            return true;
        }
        delay(1000);
        retries++;
    }
    Serial.println("❌ Failed to register on network.");
    return false;
}

/**
 * @brief Opens an HTTPS session with the remote server.
 * @return True on success, false on failure.
 */
bool openHttpsSession() {
    if (!sendATCommandCheck("AT+CHTTPSSTART", 10000, "OK")) {
        return false;
    }
    
    String cmd = "AT+CHTTPSOPSE=\"" + String(server) + "\"," + String(port);
    if (!sendATCommandCheck(cmd.c_str(), 60000, "+CHTTPSOPSE: 0")) {
        return false;
    }
    return true;
}

/**
 * @brief Closes the currently active HTTPS session.
 */
void closeHttpsSession() {
    sendATCommandCheck("AT+CHTTPSCLSE", 10000, "OK", "ERROR");
    sendATCommandCheck("AT+CHTTPSSTOP", 10000, "OK", "ERROR");
}

/**
 * @brief Sends a chunk of a file over HTTPS POST.
 * @param file The file object to read from.
 * @param filename The name of the file being uploaded.
 * @param offset The starting position (offset) of the chunk in the file.
 * @param totalSize The total size of the file.
 * @return True if the chunk was sent successfully, false otherwise.
 */
bool sendChunk(fs::File& file, const char* filename, size_t offset, size_t totalSize) {
    uint8_t chunkBuffer[CHUNK_SIZE];
    size_t bytesRead = file.read(chunkBuffer, CHUNK_SIZE);
    if (bytesRead == 0) {
        Serial.println("✅ No more bytes to read. End of file.");
        return true; 
    }

    String headers = "POST " + String(resource) + " HTTP/1.1\r\n" +
                     "Host: " + String(server) + "\r\n" +
                     "x-filename: " + String(filename) + "\r\n" +
                     "x-chunk-offset: " + String(offset) + "\r\n" +
                     "x-chunk-size: " + String(bytesRead) + "\r\n" +
                     "x-total-size: " + String(totalSize) + "\r\n" +
                     "Content-Type: application/octet-stream\r\n" +
                     "Content-Length: " + String(bytesRead) + "\r\n" +
                     "Connection: keep-alive\r\n\r\n";

    if (!sendATCommandCheck("AT+CHTTPSSEND=1", 10000, ">")) {
        Serial.println("❌ Failed to get prompt for sending headers.");
        return false;
    }

    modem.stream.print(headers);
    modem.stream.write(chunkBuffer, bytesRead);
    modem.stream.flush();

    String response;
    if (modem.waitResponse(30000, response) != 1) {
        Serial.println("❌ Timeout waiting for server response after sending chunk.");
        return false;
    }
    
    if (response.indexOf("+CHTTPSSEND: 0") != -1 && response.indexOf("200 OK") != -1) {
        Serial.println("✅ Chunk sent successfully.");
        return true;
    }

    Serial.println("❌ Server returned an error or unexpected response:");
    Serial.println(response);
    return false;
}

/**
 * @brief Main setup function, runs once on boot.
 */
void setup() {
    Serial.begin(115200);
    delay(10);

    modemSerial.begin(115200, SERIAL_8N1, MODEM_RX_PIN, MODEM_TX_PIN);

    if (!setupModem()) {
        Serial.println("❌ Modem initialization failed. Halting.");
        while (true);
    }
    
    if (modem.getSimStatus() != SIM_READY) {
        Serial.println("❌ SIM not ready. Halting.");
        while(true);
    }
    Serial.println("✅ Modem and SIM are ready.");

    printModemStatus();

    Serial.println("📡 Connecting to network...");
    if (!waitForNetwork()) {
        while (true);
    }

    if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
        Serial.println("❌ GPRS connection failed.");
        while (true);
    }
    Serial.println("✅ GPRS connected.");

    String localIP = modem.getLocalIP();
    Serial.println("Local IP: " + localIP);
    
    if (!SD.begin()) {
        Serial.println("❌ SD Card initialization failed!");
        return;
    }

    const char* targetFile = "/sigma2.wav";
    File file = SD.open(targetFile, FILE_READ);
    if (!file) {
        Serial.println("❌ Failed to open " + String(targetFile) + " for reading");
        return;
    }

    size_t fileSize = file.size();
    Serial.println("📤 Preparing to upload " + String(targetFile) + " (" + String(fileSize) + " bytes)");

    size_t offset = 0;
    while (offset < fileSize) {
        file.seek(offset);

        bool chunkSent = false;
        for (int attempt = 1; attempt <= 3; attempt++) {
            Serial.println("  Attempt " + String(attempt) + "/3 to send chunk at offset " + String(offset) + "...");
            
            if (openHttpsSession()) {
                if (sendChunk(file, "sigma2.wav", offset, fileSize)) {
                    chunkSent = true;
                    closeHttpsSession();
                    break;
                } else {
                    Serial.println("    ...sending failed.");
                }
                closeHttpsSession();
            } else {
                Serial.println("    ...connection failed.");
                closeHttpsSession(); // Ensure stopped even if open fails
            }
            delay(2000); // Wait before retrying
        }

        if (chunkSent) {
            offset += CHUNK_SIZE;
        } else {
            Serial.println("❌ Failed to upload chunk at offset " + String(offset) + " after 3 retries. Aborting.");
            break;
        }
    }

    file.close();
    Serial.println("✅ Task finished. Entering idle loop.");
}

/**
 * @brief Main loop, runs continuously after setup.
 */
void loop() {
    // Idle loop
    delay(10000);
}

    