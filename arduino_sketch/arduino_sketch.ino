
#define TINY_GSM_MODEM_SIM7600
#define TINY_GSM_DEBUG Serial

#include <HardwareSerial.h>
#include <SD.h>
#include <SPI.h>

// Pin definitions
#define MODEM_TX 17
#define MODEM_RX 16
#define SD_CS 5

// Modem settings
#define MODEM_BAUD 115200
const char apn[] = "internet";
const char gprs_user[] = "";
const char gprs_pass[] = "";

// Server settings
const char server[] = "6000-firebase-studio-1753223410587.cluster-73qgvk7hjjadkrjeyexca5ivva.cloudworkstations.dev";
const int port = 443;
const char resource[] = "/api/upload";

// File upload settings
#define CHUNK_SIZE 4096 
const char* filename = "/sigma2.wav";

// Serial setup for the modem
HardwareSerial modemSerial(1);

// Forward declarations
bool sendATCommand(const char* cmd, unsigned long timeout, const char* expected_response);
bool sendATCommand(const __FlashStringHelper* cmd, unsigned long timeout, const char* expected_response);
String sendATCommand(const char* cmd, unsigned long timeout);

void setup() {
    Serial.begin(115200);
    while (!Serial);
    delay(1000);
    
    Serial.println(F("? Booting..."));

    if (!SD.begin(SD_CS)) {
        Serial.println(F("? SD card initialization failed. Halting."));
        while (true);
    }
    Serial.println(F("? SD card ready."));

    if (!setupModem()) {
        Serial.println(F("? Modem initialization failed. Halting."));
        while (true);
    }
    
    printModemStatus();

    if (!waitForNetwork()) {
        Serial.println(F("? Failed to register on network. Halting."));
        while (true);
    }

    if (!manualGprsConnect()) {
        Serial.println(F("? GPRS connection failed. Halting."));
        while (true);
    }
    
    uploadFileInChunks(filename);

    Serial.println(F("? Task finished. Entering idle loop."));
}

void loop() {
    // Keep the device alive
    delay(1000);
}

bool setupModem() {
    Serial.println(F("? Initializing modem..."));
    modemSerial.begin(MODEM_BAUD, SERIAL_8N1, MODEM_RX, MODEM_TX);
    
    Serial.println(F("? Waiting for modem to be ready..."));
    unsigned long start_time = millis();
    while (millis() - start_time < 30000) { // 30 second timeout
        if (sendATCommand(F("AT"), 1000, "OK")) {
            Serial.println(F("? Modem is ready."));
            sendATCommand(F("ATE0"), 1000, "OK"); // Disable echo
            return true;
        }
        delay(500);
    }
    return false;
}

void printModemStatus() {
    Serial.println(F("--- Modem Status ---"));
    String imei = sendATCommand("AT+GSN", 1000);
    imei.trim();
    Serial.println("IMEI: " + imei);

    String csq = sendATCommand("AT+CSQ", 1000);
    csq.trim();
    int rssi = csq.substring(csq.indexOf(':') + 2, csq.indexOf(',')).toInt();
    Serial.println("Signal Quality: " + String(rssi));
    
    String cpin = sendATCommand("AT+CPIN?", 1000);
    cpin.trim();
    if (cpin.indexOf("READY") != -1) {
      Serial.println("SIM Status: 1");
    } else {
      Serial.println("SIM Status: 0");
    }

    String ccid = sendATCommand("AT+CCID", 1000);
    ccid.trim();
    Serial.println("CCID: " + ccid.substring(ccid.indexOf(':') + 2));
    
    String cops = sendATCommand("AT+COPS?", 1000);
    cops.trim();
    Serial.println("Operator: " + cops.substring(cops.indexOf('"') + 1, cops.lastIndexOf('"')));
    Serial.println(F("--------------------"));
}

bool waitForNetwork() {
    Serial.println(F("? Waiting for network registration..."));
    unsigned long start_time = millis();
    while (millis() - start_time < 60000) { // 60-second timeout
        String res = sendATCommand("AT+CREG?", 1000);
        res.trim();
        if (res.indexOf("+CREG: 0,1") != -1 || res.indexOf("+CREG: 0,5") != -1) {
            Serial.println("Network registration status: 1");
            Serial.println(F("? Registered on network."));
            return true;
        }
        delay(2000);
    }
    return false;
}

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

    String ip_addr_raw = sendATCommand("AT+CGPADDR=1", 5000);
    if (ip_addr_raw.indexOf("ERROR") != -1 || ip_addr_raw.indexOf("1,") == -1) {
        Serial.println(F("? Failed to get IP address."));
        return false;
    }
    ip_addr_raw.trim();
    String ip_addr = ip_addr_raw.substring(ip_addr_raw.indexOf(',') + 1);
    ip_addr.trim();
    Serial.println("? GPRS Connected. IP: " + ip_addr);
    return true;
}

bool openHttpsSession(const char* filename, size_t offset, size_t chunk_size, size_t total_size) {
    if (!sendATCommand("AT+CHTTPSSTART", 20000, "OK")) { // Increased timeout to 20s
        Serial.println(F("? Failed to start HTTPS service."));
        return false;
    }

    if (!sendATCommand((String("AT+CHTTPSPARA=\"URL\",\"") + resource + "\"").c_str(), 5000, "OK")) {
        Serial.println(F("? Failed to set URL parameter."));
        return false;
    }

    String headers = "Content-Type: application/octet-stream";
    if (!sendATCommand((String("AT+CHTTPSPARA=\"USERDATA\",\"") + headers + "\"").c_str(), 5000, "OK")) {
        Serial.println(F("? Failed to set Content-Type header."));
        return false;
    }
    
    headers = "X-Filename: " + String(filename).substring(1); // Remove leading '/'
    if (!sendATCommand((String("AT+CHTTPSPARA=\"USERDATA\",\"") + headers + "\"").c_str(), 5000, "OK")) {
        Serial.println(F("? Failed to set Filename header."));
        return false;
    }

    headers = "X-Chunk-Offset: " + String(offset);
    if (!sendATCommand((String("AT+CHTTPSPARA=\"USERDATA\",\"") + headers + "\"").c_str(), 5000, "OK")) {
        Serial.println(F("? Failed to set Chunk-Offset header."));
        return false;
    }
    
    headers = "X-Chunk-Size: " + String(chunk_size);
    if (!sendATCommand((String("AT+CHTTPSPARA=\"USERDATA\",\"") + headers + "\"").c_str(), 5000, "OK")) {
        Serial.println(F("? Failed to set Chunk-Size header."));
        return false;
    }

    headers = "X-Total-Size: " + String(total_size);
    if (!sendATCommand((String("AT+CHTTPSPARA=\"USERDATA\",\"") + headers + "\"").c_str(), 5000, "OK")) {
        Serial.println(F("? Failed to set Total-Size header."));
        return false;
    }
    
    headers = "Content-Length: " + String(chunk_size);
     if (!sendATCommand((String("AT+CHTTPSPARA=\"USERDATA\",\"") + headers + "\"").c_str(), 5000, "OK")) {
        Serial.println(F("? Failed to set Content-Length header."));
        return false;
    }

    if (!sendATCommand((String("AT+CHTTPSOPSE=\"") + server + "\"," + port).c_str(), 30000, "+CHTTPSOPSE: 0")) {
        Serial.println(F("? Failed to open HTTPS session with server."));
        return false;
    }

    return true;
}


void closeHttpsSession() {
    sendATCommand("AT+CHTTPSCLSE", 5000, "OK");
    sendATCommand("AT+CHTTPSSTOP", 5000, "OK");
}

bool sendChunk(File& file, size_t offset, size_t chunk_size) {
    file.seek(offset);
    uint8_t buffer[chunk_size];
    size_t bytesRead = file.read(buffer, chunk_size);
    if (bytesRead != chunk_size) {
        Serial.println("? File read error or EOF. Bytes read: " + String(bytesRead));
        return false;
    }

    if (!sendATCommand((String("AT+CHTTPSSEND=") + chunk_size).c_str(), 5000, ">")) {
        Serial.println(F("? Failed to initiate chunk send."));
        return false;
    }

    modemSerial.write(buffer, chunk_size);
    modemSerial.flush();

    String response = sendATCommand("", 30000);
    response.trim();
    if (response.indexOf("+CHTTPSSEND: 0") != -1 && response.indexOf("200 OK") != -1) {
        Serial.println(F("? Chunk uploaded successfully."));
        return true;
    }
    
    Serial.println("? Chunk upload failed. Server response:");
    Serial.println(response);
    return false;
}

void uploadFileInChunks(const char* filename) {
    File file = SD.open(filename);
    if (!file) {
        Serial.println("? Failed to open file: " + String(filename));
        return;
    }

    size_t total_size = file.size();
    Serial.println("? Preparing to upload " + String(filename) + " (" + String(total_size) + " bytes)");

    for (size_t offset = 0; offset < total_size; offset += CHUNK_SIZE) {
        size_t current_chunk_size = min((size_t)CHUNK_SIZE, total_size - offset);
        
        bool chunk_sent = false;
        for (int attempt = 1; attempt <= 3; ++attempt) {
            Serial.println("  Attempt " + String(attempt) + "/3 to send chunk at offset " + String(offset) + "...");
            
            if (openHttpsSession(filename, offset, current_chunk_size, total_size)) {
                Serial.println("? Uploading chunk at offset " + String(offset) + " (" + String(current_chunk_size) + " bytes)...");
                if (sendChunk(file, offset, current_chunk_size)) {
                    chunk_sent = true;
                }
            }

            closeHttpsSession(); // Always close the session after an attempt

            if (chunk_sent) {
                break; // Success, move to next chunk
            }
            
            if (attempt < 3) {
                Serial.println("    ...retrying in 5 seconds.");
                delay(5000);
            }
        }

        if (!chunk_sent) {
            Serial.println("? Failed to upload chunk at offset " + String(offset) + " after 3 retries. Aborting.");
            file.close();
            return;
        }
        delay(1000); // Small delay between chunks
    }

    Serial.println(F("? File upload completed successfully."));
    file.close();
}


// =====================================================================
// Low-level AT command handling functions
// =====================================================================

bool sendATCommand(const char* cmd, unsigned long timeout, const char* expected_response) {
    String res = "";
    res.reserve(128);

    modemSerial.println(cmd);
    Serial.print("[AT SEND] ");
    Serial.println(cmd);
    
    unsigned long start_time = millis();
    while (millis() - start_time < timeout) {
        while (modemSerial.available()) {
            char c = modemSerial.read();
            res += c;
        }
        if (res.indexOf(expected_response) != -1) {
            Serial.print("[AT RECV] ");
            Serial.println(res);
            return true;
        }
    }
    
    Serial.print("[AT RECV TIMEOUT] ");
    Serial.println(res);
    return false;
}

bool sendATCommand(const __FlashStringHelper* cmd, unsigned long timeout, const char* expected_response) {
    String res = "";
    res.reserve(128);

    modemSerial.println(cmd);
    Serial.print("[AT SEND] ");
    Serial.println(cmd);

    unsigned long start_time = millis();
    while (millis() - start_time < timeout) {
        while (modemSerial.available()) {
            char c = modemSerial.read();
            res += c;
        }
        if (res.indexOf(expected_response) != -1) {
            Serial.print("[AT RECV] ");
            Serial.println(res);
            return true;
        }
    }
    
    Serial.print("[AT RECV TIMEOUT] ");
    Serial.println(res);
    return false;
}

String sendATCommand(const char* cmd, unsigned long timeout) {
    String res = "";
    res.reserve(256);
    
    if (strlen(cmd) > 0) {
      modemSerial.println(cmd);
      Serial.print("[AT SEND] ");
      Serial.println(cmd);
    }
    
    unsigned long start_time = millis();
    while (millis() - start_time < timeout) {
        while (modemSerial.available()) {
            res += (char)modemSerial.read();
        }
    }
    return res;
}
