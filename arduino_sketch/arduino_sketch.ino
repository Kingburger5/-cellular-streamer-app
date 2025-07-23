#define TINY_GSM_MODEM_SIM7600

#include <SPI.h>
#include <SD.h>
#include <HardwareSerial.h>
#include <TinyGsmClient.h>

// Pin definitions
#define MODEM_RX 16
#define MODEM_TX 17
#define MODEM_BAUD 115200
#define SD_CS 5

// File Upload settings
#define CHUNK_SIZE 4096 // 4KB chunks

// Server configuration
const char server[] = "6000-firebase-studio-1753223410587.cluster-73qgvk7hjjadkrjeyexca5ivva.cloudworkstations.dev";
const char endpoint[] = "/api/upload";
const int port = 443;

// APN configuration
const char apn[] = "internet"; // Use "internet" for One NZ
const char user[] = "";
const char pass[] = "";

// Globals
HardwareSerial modemSerial(1);
TinyGsm modem(modemSerial);

// Forward declarations
bool sendATCommand(const char* cmd, unsigned long timeout, const char* expected_response);
bool sendATCommand(const char* cmd, unsigned long timeout, const char* expected_response1, const char* expected_response2);
String sendATCommandWithResponse(const char* cmd, unsigned long timeout);


void setup() {
    Serial.begin(115200);
    delay(10);
    Serial.println(F("Booting..."));

    if (!SD.begin(SD_CS)) {
        Serial.println(F("SD card failed or not present. Halting."));
        while (true);
    }
    Serial.println(F("SD card ready."));

    setupModem();

    if (!manualGprsConnect()) {
        Serial.println(F("GPRS connection failed. Halting."));
        while (true);
    }
    Serial.println(F("GPRS connected."));

    uploadFile("/sigma2.wav");

    Serial.println(F("Task finished. Entering idle loop."));
}

void loop() {
    delay(10000);
}

void setupModem() {
    Serial.println(F("Initializing modem..."));
    modemSerial.begin(MODEM_BAUD, SERIAL_8N1, MODEM_RX, MODEM_TX);
    
    Serial.println(F("Waiting for modem to be ready..."));
    unsigned long start = millis();
    while (millis() - start < 30000) { // 30 second timeout
        if (modem.testAT()) {
            Serial.println(F("Modem is ready."));
            
            // Turn off command echo
            sendATCommand("ATE0", 1000, "OK");
            
            // Wait for network registration
            Serial.println(F("Waiting for network registration..."));
            while (millis() - start < 60000) { // 60s timeout for network
                int regStatus = modem.getRegistrationStatus();
                Serial.print(F("Network registration status: "));
                Serial.println(regStatus);
                if (regStatus == 1 || regStatus == 5) {
                    Serial.println(F("Registered on network."));
                    return;
                }
                delay(2000);
            }
            Serial.println(F("Network registration failed."));
            return; // Failed to register
        }
        delay(500);
    }
    Serial.println(F("Modem initialization failed."));
}

bool manualGprsConnect() {
    Serial.println(F("Connecting to GPRS..."));

    // Set APN
    String cmd = "AT+CGDCONT=1,\"IP\",\"";
    cmd += apn;
    cmd += "\"";
    if (!sendATCommand(cmd.c_str(), 10000, "OK")) {
        Serial.println(F("Failed to set APN."));
        return false;
    }

    // Activate GPRS context
    if (!sendATCommand("AT+CGACT=1,1", 20000, "OK")) {
        Serial.println(F("Failed to activate GPRS context."));
        return false;
    }

    // Check for IP address
    String response = sendATCommandWithResponse("AT+CGPADDR=1", 10000);
    if (response.indexOf("+CGPADDR: 1,") != -1 && response.indexOf("0.0.0.0") == -1) {
        Serial.print(F("Local IP: "));
        Serial.println(response.substring(response.indexOf(",") + 1));
        return true;
    }

    Serial.println(F("Failed to get a valid IP address."));
    return false;
}

void uploadFile(const char *filename) {
    File file = SD.open(filename);
    if (!file) {
        Serial.print(F("Failed to open file: "));
        Serial.println(filename);
        return;
    }

    size_t fileSize = file.size();
    Serial.print(F("Preparing to upload "));
    Serial.print(filename);
    Serial.print(F(" ("));
    Serial.print(fileSize);
    Serial.println(F(" bytes)"));

    size_t offset = 0;
    while (offset < fileSize) {
        size_t chunkSize = min((size_t)CHUNK_SIZE, fileSize - offset);
        bool success = false;
        for (int retry = 0; retry < 3; retry++) {
            Serial.print(F("  Attempt "));
            Serial.print(retry + 1);
            Serial.print(F("/3 to send chunk at offset "));
            Serial.print(offset);
            Serial.println("...");
            if (sendChunk(file, offset, chunkSize, fileSize)) {
                success = true;
                break;
            }
            Serial.println(F("    ...connection failed."));
            delay(2000); // Wait before retrying
        }

        if (!success) {
            Serial.print(F("Failed to upload chunk at offset "));
            Serial.print(offset);
            Serial.println(F(" after 3 retries. Aborting."));
            file.close();
            return;
        }
        offset += chunkSize;
    }

    file.close();
    Serial.println(F("File upload finished successfully."));
}

bool sendChunk(File& file, size_t offset, size_t chunkSize, size_t totalSize) {
    if (!openHttpsSession()) {
        return false;
    }

    // Prepare and send the request URI
    String uri_cmd = "AT+CHTTPSURI=\"";
    uri_cmd += endpoint;
    uri_cmd += "\"";
    if (!sendATCommand(uri_cmd.c_str(), 10000, "OK")) {
        Serial.println(F("Failed to set HTTPS URI."));
        closeHttpsSession();
        return false;
    }

    // Prepare and send headers
    String header_cmd = "AT+CHTTPSHEAD=\"Content-Type: application/octet-stream\\r\\n";
    header_cmd += "X-Filename: sigma2.wav\\r\\n";
    header_cmd += "X-Chunk-Offset: " + String(offset) + "\\r\\n";
    header_cmd += "X-Chunk-Size: " + String(chunkSize) + "\\r\\n";
    header_cmd += "X-Total-Size: " + String(totalSize) + "\\r\\n";
    header_cmd += "\"";
    if (!sendATCommand(header_cmd.c_str(), 10000, "OK")) {
        Serial.println(F("Failed to set HTTPS headers."));
        closeHttpsSession();
        return false;
    }
    
    // Send POST request with data length
    String post_cmd = "AT+CHTTPSSEND=";
    post_cmd += chunkSize;
    if (!sendATCommand(post_cmd.c_str(), 10000, ">")) {
        Serial.println(F("Failed to initiate HTTPS send."));
        closeHttpsSession();
        return false;
    }
    
    // Read from SD and write data to modem
    uint8_t buffer[128];
    file.seek(offset);
    size_t remaining = chunkSize;
    while(remaining > 0) {
        size_t toRead = min(remaining, sizeof(buffer));
        size_t bytesRead = file.read(buffer, toRead);
        if (bytesRead == 0) break;
        modemSerial.write(buffer, bytesRead);
        remaining -= bytesRead;
    }

    // Wait for the send confirmation
    String response = sendATCommandWithResponse("", 30000); // long timeout for upload
    if (response.indexOf("+CHTTPSSEND: 0") == -1) {
        Serial.println(F("Failed to send data."));
        closeHttpsSession();
        return false;
    }
    
    // Check the response code
    response = sendATCommandWithResponse("AT+CHTTPSRECV=200", 20000);
    if (response.indexOf("200 OK") == -1 && response.indexOf("201") == -1) {
       Serial.print(F("Received non-200/201 response: "));
       Serial.println(response);
       closeHttpsSession();
       return false;
    }

    Serial.println(F("    ...chunk sent successfully."));
    closeHttpsSession();
    return true;
}


bool openHttpsSession() {
    // Start HTTPS service
    if (!sendATCommand("AT+CHTTPSSTART", 20000, "OK", "+CHTTPSSTART: 0")) {
        // If it's already started, that's ok. If it's an error, we need to stop and retry.
        String response = sendATCommandWithResponse("AT+CHTTPSSTART", 1000);
        if (response.indexOf("ERROR") != -1) {
            sendATCommand("AT+CHTTPSSTOP", 10000, "OK");
            delay(1000);
            if (!sendATCommand("AT+CHTTPSSTART", 20000, "OK", "+CHTTPSSTART: 0")) {
                Serial.println(F("Failed to start HTTPS stack on retry."));
                return false;
            }
        }
    }

    // Open session with server
    String cmd = "AT+CHTTPSOPSE=\"";
    cmd += server;
    cmd += "\",";
    cmd += port;
    if (!sendATCommand(cmd.c_str(), 60000, "+CHTTPSOPSE: 0")) {
        Serial.println(F("Failed to open HTTPS session with server."));
        return false;
    }
    return true;
}

void closeHttpsSession() {
    sendATCommand("AT+CHTTPSCLSE", 10000, "OK", "+CHTTPSCLSE: 0");
    sendATCommand("AT+CHTTPSSTOP", 10000, "OK", "+CHTTPSSTOP: 0");
}


// ============== AT Command Helpers ===============

String sendATCommandWithResponse(const char* cmd, unsigned long timeout) {
    String response = "";
    if (strlen(cmd) > 0) {
      modemSerial.println(cmd);
    }
    unsigned long start = millis();
    while (millis() - start < timeout) {
        while (modemSerial.available()) {
            response += (char)modemSerial.read();
        }
    }
    return response;
}

bool sendATCommand(const char* cmd, unsigned long timeout, const char* expected_response) {
    String response = "";
    modemSerial.println(cmd);
    unsigned long start = millis();
    while (millis() - start < timeout) {
        if (modemSerial.available()) {
            char c = modemSerial.read();
            response += c;
            if (response.indexOf(expected_response) != -1) {
                return true;
            }
        }
    }
    Serial.print(F("[DEBUG] Timeout waiting for: "));
    Serial.println(expected_response);
    Serial.print(F("[DEBUG] Received: "));
    Serial.println(response);
    return false;
}

bool sendATCommand(const char* cmd, unsigned long timeout, const char* expected_response1, const char* expected_response2) {
    String response = "";
    modemSerial.println(cmd);
    unsigned long start = millis();
    while (millis() - start < timeout) {
        if (modemSerial.available()) {
            char c = modemSerial.read();
            response += c;
            if (response.indexOf(expected_response1) != -1 || response.indexOf(expected_response2) != -1) {
                return true;
            }
        }
    }
    Serial.print(F("[DEBUG] Timeout waiting for: "));
    Serial.print(expected_response1);
    Serial.print(F(" or "));
    Serial.println(expected_response2);
    Serial.print(F("[DEBUG] Received: "));
    Serial.println(response);
    return false;
}
