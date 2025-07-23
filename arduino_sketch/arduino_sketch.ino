
#define TINY_GSM_MODEM_SIM7600
#define SerialAT XCOM

// Increase the buffer size for serial communication to handle larger AT command responses
#define TINY_GSM_RX_BUFFER 512

#include <HardwareSerial.h>
#include <SD.h>
#include <SPI.h>

// Pin definitions
#define MODEM_TX 17
#define MODEM_RX 16
#define MODEM_BAUD 115200
#define SD_CS 5

// File upload settings
#define CHUNK_SIZE 4096 // Upload in 4KB chunks for better performance
#define MAX_RETRIES 3

// Server details
const char server[] = "6000-firebase-studio-1753223410587.cluster-73qgvk7hjjadkrjeyexca5ivva.cloudworkstations.dev";
const char endpoint[] = "/api/upload";
const char apn[] = "internet"; // Use "internet" for One NZ

HardwareSerial XCOM(1);

// Forward declarations for all our functions
void printModemStatus();
bool setupModem();
bool manualGprsConnect();
bool sendFileChunks(const char* filename);
bool openHttpsSession();
void closeHttpsSession();
bool sendChunk(const uint8_t* buffer, size_t size, size_t offset, size_t totalSize, const char* filename);
String sendATCommand(const char* cmd, unsigned long timeout, const char* expected_response);
String sendATCommand(const __FlashStringHelper* cmd, unsigned long timeout, const char* expected_response);


void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println(F("? Booting..."));

    if (!SD.begin(SD_CS)) {
        Serial.println(F("? SD card failed. Halting."));
        while (true);
    }
    Serial.println(F("? SD card ready."));

    if (!setupModem()) {
        Serial.println(F("? Modem initialization failed. Halting."));
        while (true);
    }
    
    printModemStatus();

    if (!manualGprsConnect()) {
        Serial.println(F("? GPRS connection failed. Halting."));
        while(true);
    }

    sendFileChunks("/sigma2.wav");

    Serial.println(F("? Task finished. Entering idle loop."));
}

void loop() {
    // Keep the device alive but do nothing
    delay(1000);
}

bool setupModem() {
    Serial.println(F("? Initializing modem..."));
    XCOM.begin(MODEM_BAUD, SERIAL_8N1, MODEM_RX, MODEM_TX);
    delay(3000); // Wait for modem to power on

    Serial.println(F("? Waiting for modem to be ready..."));
    unsigned long start = millis();
    while (millis() - start < 30000) { // 30-second timeout
        if (sendATCommand(F("AT"), 1000, "OK") != "") {
            Serial.println(F("? Modem is ready."));
            sendATCommand(F("ATE0"), 1000, "OK"); // Disable command echo
            return true;
        }
        delay(500);
    }
    return false;
}

void printModemStatus() {
    Serial.println(F("--- Modem Status ---"));
    String imei = sendATCommand(F("AT+GSN"), 1000, "OK");
    imei.replace("AT+GSN\r\n", "");
    imei.replace("\r\nOK", "");
    imei.trim();
    Serial.println("IMEI: " + imei);

    String csq = sendATCommand(F("AT+CSQ"), 1000, "OK");
    if (csq.indexOf("+CSQ:") != -1) {
        csq = csq.substring(csq.indexOf("+CSQ:") + 6);
        csq = csq.substring(0, csq.indexOf(','));
        csq.trim();
        Serial.println("Signal Quality: " + csq);
    }
    
    String cpin = sendATCommand(F("AT+CPIN?"), 1000, "OK");
     if (cpin.indexOf("+CPIN: READY") != -1) {
        Serial.println("SIM Status: 1");
    } else {
        Serial.println("SIM Status: 0");
    }

    String ccid = sendATCommand(F("AT+CCID"), 2000, "OK");
    if(ccid.indexOf("+CCID:") != -1){
      ccid = ccid.substring(ccid.indexOf("+CCID:") + 7);
      ccid.replace("\r\n\r\nOK", "");
      ccid.trim();
      Serial.println("CCID: " + ccid);
    }

    String cops = sendATCommand(F("AT+COPS?"), 5000, "OK");
    if (cops.indexOf("+COPS:") != -1) {
        cops = cops.substring(cops.indexOf("\"") + 1);
        cops = cops.substring(0, cops.indexOf("\""));
        cops.trim();
        Serial.println("Operator: " + cops);
    }
    Serial.println(F("--------------------"));
}

bool manualGprsConnect() {
    Serial.println(F("? Waiting for network registration..."));
    unsigned long start = millis();
    while (millis() - start < 60000) { // 60-second timeout
        String res = sendATCommand(F("AT+CREG?"), 2000, "+CREG:");
        if (res.indexOf("+CREG: 0,1") != -1 || res.indexOf("+CREG: 0,5") != -1) {
            Serial.println("Network registration status: 1");
            Serial.println(F("? Registered on network."));
            break;
        }
        delay(2000);
    }

    Serial.println(F("? Connecting to GPRS..."));

    // Set APN
    String cmd = "AT+CGDCONT=1,\"IP\",\"" + String(apn) + "\"";
    if (sendATCommand(cmd.c_str(), 5000, "OK") == "") return false;

    // Activate GPRS context
    if (sendATCommand(F("AT+CGACT=1,1"), 30000, "OK") == "") {
         Serial.println(F("? Failed to activate GPRS context."));
         return false;
    }
    
    // Check for IP address
    unsigned long ip_check_start = millis();
    while(millis() - ip_check_start < 30000) { // 30 second timeout for IP
        String res = sendATCommand(F("AT+CGPADDR=1"), 5000, "+CGPADDR:");
        if (res.indexOf("+CGPADDR: 1,") != -1) {
            res = res.substring(res.indexOf("+CGPADDR: 1,") + 12);
            res.trim();
            Serial.println("? GPRS Connected. IP: " + res);
            return true;
        }
        delay(1000);
    }

    return false;
}

bool sendFileChunks(const char* filename) {
    File file = SD.open(filename);
    if (!file) {
        Serial.println(F("? Failed to open file."));
        return false;
    }

    size_t totalSize = file.size();
    size_t offset = 0;
    
    Serial.printf("? Preparing to upload %s (%d bytes)\n", filename, totalSize);

    while (offset < totalSize) {
        size_t chunkSize = min((size_t)CHUNK_SIZE, totalSize - offset);
        uint8_t buffer[chunkSize];
        file.seek(offset);
        file.read(buffer, chunkSize);

        bool success = false;
        for (int retry = 1; retry <= MAX_RETRIES; ++retry) {
            Serial.printf("  Attempt %d/%d to send chunk at offset %d...\n", retry, MAX_RETRIES, offset);
            if (sendChunk(buffer, chunkSize, offset, totalSize, filename)) {
                success = true;
                break;
            }
            Serial.println(F("    ...upload attempt failed."));
            delay(2000); // Wait before retrying
        }

        if (!success) {
            Serial.printf("? Failed to upload chunk at offset %d after %d retries. Aborting.\n", offset, MAX_RETRIES);
            file.close();
            return false;
        }

        offset += chunkSize;
    }

    file.close();
    Serial.println(F("? File upload complete."));
    return true;
}

bool openHttpsSession() {
    // Start HTTPS service, allow up to 20 seconds for TLS handshake
    if (sendATCommand(F("AT+CHTTPSSTART"), 20000, "OK") == "") {
        Serial.println(F("? HTTPS start failed."));
        closeHttpsSession(); // Attempt cleanup
        return false;
    }

    // Open HTTPS session with the server, allow up to 30 seconds
    if (sendATCommand(("AT+CHTTPSOPSE=\"" + String(server) + "\",443").c_str(), 30000, "OK") == "") {
        Serial.println(F("? Failed to open HTTPS session with server."));
        closeHttpsSession();
        return false;
    }

    // Set the URL parameter, allow up to 20 seconds for potential DNS lookup
    String cmd = "AT+CHTTPSPARA=\"URL\",\"" + String(endpoint) + "\"";
    if (sendATCommand(cmd.c_str(), 20000, "OK") == "") {
        Serial.println(F("? Failed to set URL parameter."));
        closeHttpsSession();
        return false;
    }
    
    return true;
}


void closeHttpsSession() {
    sendATCommand(F("AT+CHTTPSCLSE"), 10000, "OK"); // Close the session
    sendATCommand(F("AT+CHTTPSSTOP"), 10000, "OK");  // Stop the HTTPS service
}

bool sendChunk(const uint8_t* buffer, size_t size, size_t offset, size_t totalSize, const char* filename) {
    if (!openHttpsSession()) {
        return false;
    }

    // Set Content-Type Header
    String cmd = "AT+CHTTPSPARA=\"USERDATA\",\"Content-Type: application/octet-stream\"";
    if (sendATCommand(cmd.c_str(), 5000, "OK") == "") {
        Serial.println(F("? Failed to set Content-Type header."));
        closeHttpsSession();
        return false;
    }

    // Set Custom Headers
    cmd = "AT+CHTTPSPARA=\"USERDATA\",\"X-Filename: " + String(filename) + "\"";
    if (sendATCommand(cmd.c_str(), 5000, "OK") == "") {
        Serial.println(F("? Failed to set Filename header."));
        closeHttpsSession();
        return false;
    }
    cmd = "AT+CHTTPSPARA=\"USERDATA\",\"X-Chunk-Offset: " + String(offset) + "\"";
    if (sendATCommand(cmd.c_str(), 5000, "OK") == "") {
        Serial.println(F("? Failed to set Offset header."));
        closeHttpsSession();
        return false;
    }
    cmd = "AT+CHTTPSPARA=\"USERDATA\",\"X-Chunk-Size: " + String(size) + "\"";
    if (sendATCommand(cmd.c_str(), 5000, "OK") == "") {
        Serial.println(F("? Failed to set Chunk-Size header."));
        closeHttpsSession();
        return false;
    }
    cmd = "AT+CHTTPSPARA=\"USERDATA\",\"X-Total-Size: " + String(totalSize) + "\"";
    if (sendATCommand(cmd.c_str(), 5000, "OK") == "") {
        Serial.println(F("? Failed to set Total-Size header."));
        closeHttpsSession();
        return false;
    }
     cmd = "AT+CHTTPSPARA=\"USERDATA\",\"Content-Length: " + String(size) + "\"";
    if (sendATCommand(cmd.c_str(), 5000, "OK") == "") {
        Serial.println(F("? Failed to set Content-Length header."));
        closeHttpsSession();
        return false;
    }

    // Send the POST request with the chunk size
    cmd = "AT+CHTTPSPOST=" + String(size);
    if (sendATCommand(cmd.c_str(), 5000, ">") == "") {
        Serial.println(F("? Failed to initiate POST request."));
        closeHttpsSession();
        return false;
    }

    // Write the binary data
    SerialAT.write(buffer, size);
    SerialAT.flush();

    // Wait for the server's response after sending data
    String response = sendATCommand("", 15000, "+CHTTPSPOST: 0");
    if (response == "") {
         Serial.println(F("? Did not receive server response after POST."));
         closeHttpsSession();
         return false;
    }

    // Now explicitly ask for the data from the server
    String recvResponse = sendATCommand("AT+CHTTPSRECV", 15000, "+CHTTPSRECV: DATA");
    if (recvResponse.indexOf("HTTP/1.1 200") == -1) {
        Serial.println("? Chunk upload failed. Server response:");
        Serial.println(recvResponse);
        closeHttpsSession();
        return false;
    }
    
    Serial.println(F("? Chunk uploaded successfully."));
    closeHttpsSession();
    return true;
}


String sendATCommand(const char* cmd, unsigned long timeout, const char* expected_response) {
    String res = "";
    if (strlen(cmd) > 0) {
        Serial.print("[AT SEND] ");
        Serial.println(cmd);
        SerialAT.println(cmd);
    }

    unsigned long start = millis();
    while (millis() - start < timeout) {
        while (SerialAT.available()) {
            res += (char)SerialAT.read();
        }
        if (res.indexOf(expected_response) != -1) {
            break;
        }
    }
    
    // Clean up and print response
    res.trim();
    if (res != "") {
        Serial.print("[AT RECV] ");
        Serial.println(res);
    } else if (strlen(cmd) > 0) {
        Serial.println("[AT RECV TIMEOUT]");
    }

    if (res.indexOf(expected_response) != -1) {
        return res;
    }
    return "";
}

String sendATCommand(const __FlashStringHelper* cmd, unsigned long timeout, const char* expected_response) {
    String res = "";
    Serial.print("[AT SEND] ");
    Serial.println(cmd);
    SerialAT.println(cmd);

    unsigned long start = millis();
    while (millis() - start < timeout) {
        while (SerialAT.available()) {
            res += (char)SerialAT.read();
        }
        if (res.indexOf(expected_response) != -1) {
            break;
        }
    }
    
    res.trim();
    if (res != "") {
        Serial.print("[AT RECV] ");
        Serial.println(res);
    } else {
        Serial.println("[AT RECV TIMEOUT]");
    }
    
    if (res.indexOf(expected_response) != -1) {
        return res;
    }
    return "";
}
