// Define the modem type and activate debug functionality
#define TINY_GSM_MODEM_SIM7600
#define TINY_GSM_DEBUG Serial

// Include necessary libraries
#include <HardwareSerial.h>
#include <TinyGsmClient.h>
#include <SD.h>
#include <SPI.h>

// Pin definitions and constants
#define MODEM_TX 17
#define MODEM_RX 16
#define MODEM_BAUD 115200
#define SD_CS 5
#define CHUNK_SIZE 4096 // Use a larger chunk size for faster uploads
#define MAX_UPLOAD_RETRIES 3

// Hardware and server configuration
HardwareSerial xcom(1);
TinyGsm modem(xcom);

const char server[] = "6000-firebase-studio-1753223410587.cluster-73qgvk7hjjadkrjeyexca5ivva.cloudworkstations.dev";
const char endpoint[] = "/api/upload";
const char apn[] = "internet"; // Use "internet" for One NZ

// --- Forward Declarations ---
// These are necessary because the functions are defined after they are first used.
bool sendATCommand(const char* cmd, unsigned long timeout, const char* expected_response1, const char* expected_response2 = nullptr);
bool sendATCommand(const __FlashStringHelper* cmd, unsigned long timeout, const char* expected_response1, const char* expected_response2 = nullptr);
bool sendChunk(File& file, size_t offset, size_t totalSize);
void printModemStatus();
bool manualGprsConnect();
bool openHttpsSession();
void closeHttpsSession();
bool setRequestHeaders(const char* filename, size_t offset, size_t chunkSize, size_t totalSize);


// =================================================================
// SETUP FUNCTION
// =================================================================
void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println(F("? Booting..."));

    // Initialize SD card
    if (!SD.begin(SD_CS)) {
        Serial.println(F("? SD card failed. Halting."));
        while (true);
    }
    Serial.println(F("? SD card ready."));

    // Initialize modem
    Serial.println(F("? Initializing modem..."));
    xcom.begin(MODEM_BAUD, SERIAL_8N1, MODEM_RX, MODEM_TX);
    delay(3000); // Wait for the modem to power on

    // Wait for the modem to become responsive
    Serial.println(F("? Waiting for modem to be ready..."));
    while (!modem.testAT(1000)) {
        Serial.print(F("."));
        delay(1000);
    }
    Serial.println(F("\n? Modem is ready."));

    printModemStatus();

    // Wait for network registration
    Serial.println(F("? Waiting for network registration..."));
    while (!modem.waitForNetwork(10000)) {
        Serial.print(F("."));
        sendATCommand(F("AT+CREG?"), 1000, "OK");
    }
    String regStatus = modem.getRegistrationStatus();
    Serial.print(F("Network registration status: "));
    Serial.println(regStatus);

    if (!regStatus.endsWith(",1") && !regStatus.endsWith(",5")) {
        Serial.println(F("? Could not register on network. Halting."));
        while (true);
    }
    Serial.println(F("? Registered on network."));


    // Connect to GPRS
    if (!manualGprsConnect()) {
        Serial.println(F("? GPRS connection failed. Halting."));
        while (true);
    }
    
    // Begin file upload process
    const char* filename = "/sigma2.wav";
    File file = SD.open(filename);
    if (!file) {
        Serial.print(F("? Failed to open file: "));
        Serial.println(filename);
        return;
    }
    
    size_t totalSize = file.size();
    Serial.print(F("? Preparing to upload "));
    Serial.print(filename);
    Serial.print(F(" ("));
    Serial.print(totalSize);
    Serial.println(F(" bytes)"));

    size_t offset = 0;
    while (offset < totalSize) {
        if (sendChunk(file, offset, totalSize)) {
            offset += CHUNK_SIZE;
        } else {
            Serial.println(F("? Upload failed after multiple retries. Aborting."));
            break; // Exit the loop on persistent failure
        }
    }

    file.close();
    Serial.println(F("? Upload process finished."));
}

// =================================================================
// MAIN LOOP
// =================================================================
void loop() {
    // Nothing here, all work is done in setup
}


// =================================================================
// HELPER FUNCTIONS
// =================================================================

/**
 * @brief Manually connects to the GPRS network using AT commands.
 * @return True if successful, false otherwise.
 */
bool manualGprsConnect() {
    Serial.println(F("? Connecting to GPRS..."));
    
    String cmd = "AT+CGDCONT=1,\"IP\",\"";
    cmd += apn;
    cmd += "\"";
    if (!sendATCommand(cmd.c_str(), 5000, "OK")) return false;

    if (!sendATCommand(F("AT+CGACT=1,1"), 10000, "OK")) return false;

    delay(1000); // Small delay to allow IP address to be assigned

    if (!sendATCommand(F("AT+CGPADDR=1"), 5000, "OK")) {
        Serial.println(F("? Failed to get IP address."));
        return false;
    }
    Serial.print(F("? GPRS Connected. IP: "));
    Serial.println(modem.getLocalIP());
    return true;
}


/**
 * @brief Opens a secure HTTPS session with the server.
 * @return True if successful, false otherwise.
 */
bool openHttpsSession() {
    if (!sendATCommand(F("AT+CHTTPSSTART"), 20000, "OK", "+CHTTPSSTART: 0")) return false;

    String cmd = "AT+CHTTPSOPSE=\"";
    cmd += server;
    cmd += "\",443";
    if (!sendATCommand(cmd.c_str(), 20000, "+CHTTPSOPSE: 0")) {
        Serial.println(F("? Failed to open HTTPS session."));
        return false;
    }

    if (!sendATCommand(F("AT+CHTTPSPARA=\"URL\",\"/api/upload\""), 20000, "OK")) {
         Serial.println(F("? Failed to set URL parameter."));
        return false;
    }
    
    return true;
}

/**
 * @brief Sets the necessary HTTP headers for the file chunk upload.
 * @return True if successful, false otherwise.
 */
bool setRequestHeaders(const char* filename, size_t offset, size_t chunkSize, size_t totalSize) {
    String cmd;
    
    cmd = "AT+CHTTPSPARA=\"USERDATA\",\"Content-Type: application/octet-stream\"";
    if (!sendATCommand(cmd.c_str(), 5000, "OK")) return false;

    cmd = "AT+CHTTPSPARA=\"USERDATA\",\"X-Filename: " + String(filename) + "\"";
    if (!sendATCommand(cmd.c_str(), 5000, "OK")) return false;

    cmd = "AT+CHTTPSPARA=\"USERDATA\",\"X-Chunk-Offset: " + String(offset) + "\"";
    if (!sendATCommand(cmd.c_str(), 5000, "OK")) return false;

    cmd = "AT+CHTTPSPARA=\"USERDATA\",\"X-Chunk-Size: " + String(chunkSize) + "\"";
    if (!sendATCommand(cmd.c_str(), 5000, "OK")) return false;
    
    cmd = "AT+CHTTPSPARA=\"USERDATA\",\"X-Total-Size: " + String(totalSize) + "\"";
    if (!sendATCommand(cmd.c_str(), 5000, "OK")) return false;

    cmd = "AT+CHTTPSPARA=\"CONTENTLEN\"," + String(chunkSize);
    if (!sendATCommand(cmd.c_str(), 5000, "OK")) return false;

    return true;
}


/**
 * @brief Closes the current HTTPS session and stops the service.
 */
void closeHttpsSession() {
    sendATCommand(F("AT+CHTTPSCLSE"), 10000, "+CHTTPSCLSE: 0");
    sendATCommand(F("AT+CHTTPSSTOP"), 10000, "+CHTTPSSTOP: 0");
}


/**
 * @brief Prints key modem status information to the Serial monitor.
 */
void printModemStatus() {
    Serial.println(F("--- Modem Status ---"));
    sendATCommand(F("AT+GSN"), 1000, "OK");
    sendATCommand(F("AT+CSQ"), 1000, "OK");
    sendATCommand(F("AT+CPIN?"), 1000, "OK");
    sendATCommand(F("AT+CCID"), 1000, "OK");
    sendATCommand(F("AT+COPS?"), 1000, "OK");
    Serial.println(F("--------------------"));
}

/**
 * @brief Sends a single chunk of a file with retries.
 * @param file The file object to read from.
 * @param offset The starting position in the file to read the chunk from.
 * @param totalSize The total size of the file.
 * @return True if the chunk was sent successfully, false otherwise.
 */
bool sendChunk(File& file, size_t offset, size_t totalSize) {
    for (int attempt = 1; attempt <= MAX_UPLOAD_RETRIES; ++attempt) {
        Serial.print(F("  Attempt "));
        Serial.print(attempt);
        Serial.print(F("/"));
        Serial.print(MAX_UPLOAD_RETRIES);
        Serial.print(F(" to send chunk at offset "));
        Serial.print(offset);
        Serial.println(F("..."));

        if (!openHttpsSession()) {
            Serial.println(F("? Failed to open session. Retrying..."));
            closeHttpsSession();
            delay(2000);
            continue;
        }

        size_t chunkSize = min((size_t)CHUNK_SIZE, totalSize - offset);
        
        if (!setRequestHeaders(file.name(), offset, chunkSize, totalSize)) {
            Serial.println(F("? Failed to set headers. Retrying..."));
            closeHttpsSession();
            delay(2000);
            continue;
        }

        if (!sendATCommand(F("AT+CHTTPSD=POST,1"), 10000, ">")) {
             Serial.println(F("? Failed to get prompt for data send."));
             closeHttpsSession();
             delay(2000);
             continue;
        }

        // Read chunk from SD and write to modem
        file.seek(offset);
        uint8_t buffer[256];
        size_t bytesSent = 0;
        while(bytesSent < chunkSize) {
            size_t bytesToRead = min((size_t)sizeof(buffer), chunkSize - bytesSent);
            size_t bytesRead = file.read(buffer, bytesToRead);
            if (bytesRead == 0) break;
            xcom.write(buffer, bytesRead);
            bytesSent += bytesRead;
        }
        
        // Wait for the server to process the chunk
        String response;
        if (sendATCommand((const char*)nullptr, 30000, "+CHTTPS: 200", response)) {
             Serial.println(F("? Chunk uploaded successfully."));
             closeHttpsSession();
             return true;
        } else {
             Serial.println(F("? Chunk upload failed. Server response:"));
             Serial.println(response);
             closeHttpsSession();
             delay(2000);
        }
    }

    return false; // Failed after all retries
}



/**
 * @brief Sends an AT command and waits for an expected response.
 * @param cmd The command to send.
 * @param timeout The time to wait for a response.
 * @param expected_response1 The first expected response string.
 * @param expected_response2 (Optional) The second expected response string (for URCs).
 * @return True if an expected response is received, false otherwise.
 */
bool sendATCommand(const char* cmd, unsigned long timeout, const char* expected_response1, const char* expected_response2) {
    String response;
    return sendATCommand(cmd, timeout, expected_response1, response, expected_response2);
}

/**
 * @brief Overload for FlashStringHelper.
 */
bool sendATCommand(const __FlashStringHelper* cmd, unsigned long timeout, const char* expected_response1, const char* expected_response2) {
    String response;
    return sendATCommand(cmd, timeout, expected_response1, response, expected_response2);
}

/**
 * @brief Sends an AT command and captures the full response.
 * @return True if an expected response is received, false otherwise.
 */
bool sendATCommand(const char* cmd, unsigned long timeout, const char* expected_response, String& response_str, const char* expected_response2) {
    if (cmd) {
        Serial.print(F("[AT SEND] "));
        Serial.println(cmd);
        xcom.println(cmd);
    }
    
    unsigned long start = millis();
    response_str = "";
    while (millis() - start < timeout) {
        while (xcom.available()) {
            char c = xcom.read();
            response_str += c;
        }
        if (expected_response && response_str.indexOf(expected_response) != -1) {
            Serial.print(F("[AT RECV] "));
            Serial.println(response_str);
            return true;
        }
        if (expected_response2 && response_str.indexOf(expected_response2) != -1) {
            Serial.print(F("[AT RECV] "));
            Serial.println(response_str);
            return true;
        }
    }
    
    Serial.print(F("[AT RECV TIMEOUT] "));
    Serial.println(response_str);
    return false;
}

/**
 * @brief Overload for FlashStringHelper.
 */
bool sendATCommand(const __FlashStringHelper* cmd, unsigned long timeout, const char* expected_response, String& response_str, const char* expected_response2) {
     if (cmd) {
        Serial.print(F("[AT SEND] "));
        Serial.println(cmd);
        xcom.println(cmd);
    }
    
    unsigned long start = millis();
    response_str = "";
    while (millis() - start < timeout) {
        while (xcom.available()) {
            char c = xcom.read();
            response_str += c;
        }
        if (expected_response && response_str.indexOf(expected_response) != -1) {
            Serial.print(F("[AT RECV] "));
            Serial.println(response_str);
            return true;
        }
        if (expected_response2 && response_str.indexOf(expected_response2) != -1) {
            Serial.print(F("[AT RECV] "));
            Serial.println(response_str);
            return true;
        }
    }
    
    Serial.print(F("[AT RECV TIMEOUT] "));
    Serial.println(response_str);
    return false;
}
