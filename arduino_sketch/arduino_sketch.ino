
#define TINY_GSM_MODEM_SIM7600

#include <SD.h>
#include <SPI.h>
#include <HardwareSerial.h>

// --- Pin Definitions ---
// DO NOT CHANGE THESE
#define MODEM_RX 16
#define MODEM_TX 17
#define SD_CS_PIN 5

// --- Serial & Connection Configuration ---
#define MODEM_BAUD 115200
#define SERIAL_BAUD 115200
#define CHUNK_SIZE 1024
#define MAX_RETRIES 3
#define COMMAND_TIMEOUT 10000 // 10 seconds for most commands
#define HTTP_ACTION_TIMEOUT 30000 // 30 seconds for HTTP action

// --- APN Configuration ---
const char apn[] = "internet"; // APN for One NZ
const char gprsUser[] = "";
const char gprsPass[] = "";

// --- Server Configuration ---
const char server[] = "6000-firebase-studio-1753223410587.cluster-73qgvk7hjjadkrjeyexca5ivva.cloudworkstations.dev";
const char endpoint[] = "/api/upload";

// --- Global Objects ---
HardwareSerial modemSerial(1);

// Forward declarations
bool sendATCommand(const String& cmd, unsigned long timeout, const char* expectedResponse);
bool sendATCommand(const String& cmd, unsigned long timeout);
String sendATCommandWithResponse(const String& cmd, unsigned long timeout);


void setup() {
    Serial.begin(SERIAL_BAUD);
    delay(1000);
    Serial.println(F("\n? Booting device..."));

    // --- Initialize SD Card ---
    if (!SD.begin(SD_CS_PIN)) {
        Serial.println(F("? SD Card initialization failed. Halting."));
        while (true);
    }
    Serial.println(F("? SD Card ready."));

    // --- Initialize Modem ---
    if (!setupModem()) {
        Serial.println(F("? Modem initialization failed. Halting."));
        while (true);
    }
    printModemStatus();
    
    // --- Connect to GPRS ---
    if (!manualGprsConnect()) {
        Serial.println(F("? GPRS connection failed. Halting."));
        while (true);
    }
    Serial.println(F("? GPRS Connected."));

    // --- Upload File ---
    uploadFile("/sigma2.wav");
    
    Serial.println(F("? Task finished. Entering idle loop."));
}

void loop() {
    // Keep idle to prevent watchdog reset
    delay(1000);
}

bool setupModem() {
    Serial.println(F("? Initializing modem..."));
    modemSerial.begin(MODEM_BAUD, SERIAL_8N1, MODEM_RX, MODEM_TX);

    Serial.println(F("Waiting for modem to be ready..."));
    unsigned long start = millis();
    while (millis() - start < 30000) { // 30-second timeout
        if (sendATCommand("AT", 1000, "OK")) {
            Serial.println(F("? Modem is ready."));
            sendATCommand("ATE0", 1000); // Disable echo
            return true;
        }
        delay(500);
    }
    return false;
}

void printModemStatus() {
    Serial.println(F("--- Modem Status ---"));
    String imei = sendATCommandWithResponse("AT+CIMI", 1000);
    imei.trim();
    if (imei.length() > 0) {
        Serial.println("IMEI: " + imei);
    }

    String csq = sendATCommandWithResponse("AT+CSQ", 1000);
    if (csq.indexOf("+CSQ:") != -1) {
        csq.replace("+CSQ: ", "");
        csq.trim();
        int rssi = csq.substring(0, csq.indexOf(',')).toInt();
        Serial.println("Signal Quality: " + String(rssi));
    }

    String ccid = sendATCommandWithResponse("AT+CCID", 1000);
     if (ccid.indexOf("+CCID:") != -1) {
        ccid.replace("+CCID: ", "");
        ccid.trim();
        Serial.println("CCID: " + ccid);
    }

    String cops = sendATCommandWithResponse("AT+COPS?", 5000);
    if (cops.indexOf("+COPS:") != -1) {
        cops.substring(cops.indexOf("\"") + 1, cops.lastIndexOf("\""));
        Serial.println("Operator: " + cops);
    }
    Serial.println(F("--------------------"));
}

bool manualGprsConnect() {
    Serial.println(F("? Connecting to network..."));

    // Wait for network registration
    unsigned long start = millis();
    while (millis() - start < 60000) { // 60-second timeout
        String regStatus = sendATCommandWithResponse("AT+CREG?", 2000);
        if (regStatus.indexOf("+CREG: 0,1") != -1 || regStatus.indexOf("+CREG: 0,5") != -1) {
            Serial.println(F("? Registered on network."));
            break;
        }
        delay(2000);
    }

    Serial.println(F("? Setting APN..."));
    if (!sendATCommand("AT+CGDCONT=1,\"IP\",\"" + String(apn) + "\"", 5000, "OK")) {
        Serial.println(F("? Failed to set APN."));
        return false;
    }

    Serial.println(F("? Activating GPRS context..."));
    if (!sendATCommand("AT+CGACT=1,1", 30000, "OK")) {
        Serial.println(F("? Failed to activate GPRS context."));
        return false;
    }
    
    // Check for IP address
    start = millis();
    while(millis() - start < 30000) {
        String ipAddr = sendATCommandWithResponse("AT+CGPADDR=1", 2000);
        if (ipAddr.indexOf("0.0.0.0") == -1 && ipAddr.indexOf("+CGPADDR:") != -1) {
            Serial.println("Local IP: " + ipAddr.substring(ipAddr.indexOf(":") + 1).trimmed());
            return true;
        }
        delay(1000);
    }

    return false;
}

bool openHttpsSession() {
    if (!sendATCommand("AT+CHTTPSSTART", COMMAND_TIMEOUT, "OK")) {
        // If it fails, maybe it's already open. Try to stop and restart.
        sendATCommand("AT+CHTTPSSTOP", COMMAND_TIMEOUT);
        if (!sendATCommand("AT+CHTTPSSTART", COMMAND_TIMEOUT, "OK")) {
             Serial.println(F("? ERROR: Failed to start HTTPS service."));
             return false;
        }
    }

    String cmd = "AT+CHTTPSOPSE=\"" + String(server) + "\",443";
    if (!sendATCommand(cmd, 20000, "+CHTTPSOPSE: 0")) {
        Serial.println(F("? ERROR: Failed to open HTTPS session."));
        closeHttpsSession();
        return false;
    }
    return true;
}

void closeHttpsSession() {
    sendATCommand("AT+CHTTPSCLSE", COMMAND_TIMEOUT);
    sendATCommand("AT+CHTTPSSTOP", COMMAND_TIMEOUT);
}

void uploadFile(const char* filename) {
    File file = SD.open(filename, FILE_READ);
    if (!file) {
        Serial.println(F("? ERROR: Failed to open file for reading."));
        return;
    }

    size_t fileSize = file.size();
    Serial.printf("? Preparing to upload %s (%d bytes)\n", filename, fileSize);

    uint8_t buffer[CHUNK_SIZE];
    size_t offset = 0;

    while (offset < fileSize) {
        bool chunkSent = false;
        for (int i = 0; i < MAX_RETRIES; i++) {
            Serial.printf("  Attempt %d/%d to send chunk at offset %d...\n", i + 1, MAX_RETRIES, offset);
            
            if (sendChunk(file, filename, fileSize, offset, buffer)) {
                chunkSent = true;
                break;
            } else {
                Serial.println(F("    ...chunk upload failed. Retrying."));
                delay(2000); // Wait before retrying
            }
        }

        if (chunkSent) {
            offset += CHUNK_SIZE;
        } else {
            Serial.printf("? ERROR: Failed to upload chunk at offset %d after %d retries. Aborting.\n", offset, MAX_RETRIES);
            file.close();
            return;
        }
    }
    file.close();
    Serial.println(F("? File upload complete."));
}

bool sendChunk(File& file, const char* filename, size_t fileSize, size_t offset, uint8_t* buffer) {
    if (!openHttpsSession()) {
        return false;
    }

    size_t bytesToRead = min((size_t)CHUNK_SIZE, fileSize - offset);
    file.seek(offset);
    size_t bytesRead = file.read(buffer, bytesToRead);
    if(bytesRead != bytesToRead) {
        Serial.println("? ERROR: SD card read error.");
        closeHttpsSession();
        return false;
    }

    // Set HTTP parameters
    String headers = "X-Filename: " + String(filename) + "\r\n" +
                     "X-Chunk-Offset: " + String(offset) + "\r\n" +
                     "X-Chunk-Size: " + String(bytesRead) + "\r\n" +
                     "X-Total-Size: " + String(fileSize);

    sendATCommand("AT+CHTTPSPARA=\"URL\",\"" + String(endpoint) + "\"", COMMAND_TIMEOUT, "OK");
    sendATCommand("AT+CHTTPSPARA=\"CONTENT\",\"application/octet-stream\"", COMMAND_TIMEOUT, "OK");
    sendATCommand("AT+CHTTPSPARA=\"USERDATA\",\"" + headers + "\"", COMMAND_TIMEOUT, "OK");

    // Send data
    if (!sendATCommand("AT+CHTTPSDAS=1," + String(COMMAND_TIMEOUT), COMMAND_TIMEOUT, ">")) {
         Serial.println(F("? ERROR: Failed to enter data send mode."));
         closeHttpsSession();
         return false;
    }
    
    modemSerial.write(buffer, bytesRead);
    modemSerial.flush();
    
    String dasResponse = sendATCommandWithResponse("", 20000); // Wait for OK after data
    if (dasResponse.indexOf("OK") == -1) {
        Serial.println(F("? ERROR: Did not get OK after sending data."));
        closeHttpsSession();
        return false;
    }
    
    // Perform HTTP POST action
    if (!sendATCommand("AT+CHTTPSACTION=1", HTTP_ACTION_TIMEOUT, "+CHTTPSACTION: 1,200")) {
         Serial.println(F("? ERROR: HTTP POST action failed."));
         closeHttpsSession();
         return false;
    }
    
    Serial.printf("  Chunk at offset %d sent successfully.\n", offset);
    
    closeHttpsSession();
    return true;
}

// --- Low-Level AT Command Helpers ---

bool sendATCommand(const String& cmd, unsigned long timeout, const char* expectedResponse) {
    String response = sendATCommandWithResponse(cmd, timeout);
    return response.indexOf(expectedResponse) != -1;
}

bool sendATCommand(const String& cmd, unsigned long timeout) {
    return sendATCommand(cmd, timeout, "OK");
}

String sendATCommandWithResponse(const String& cmd, unsigned long timeout) {
    String response = "";
    if (cmd.length() > 0) {
        modemSerial.println(cmd);
    }
    
    unsigned long start = millis();
    while (millis() - start < timeout) {
        while (modemSerial.available()) {
            char c = modemSerial.read();
            response += c;
        }
    }
    // For debugging the raw responses
    // if (response.length() > 0) {
    //     Serial.print("[RAW R] ");
    //     Serial.println(response);
    // }
    return response;
}
