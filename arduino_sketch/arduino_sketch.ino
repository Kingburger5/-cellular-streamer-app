
#define TINY_GSM_MODEM_SIM7600
#define TINY_GSM_DEBUG Serial

#include <TinyGsmClient.h>
#include <SD.h>
#include <SPI.h>

// Pin definitions
#define MODEM_TX 17
#define MODEM_RX 16
#define MODEM_BAUD 115200
#define SD_CS 5

// File Upload settings
#define CHUNK_SIZE 8192 // 8KB chunk size for more efficient transfer

// Hardware Serial for modem communication
HardwareSerial modemSerial(1);

// TinyGSM modem object
TinyGsm modem(modemSerial);

// Server details
const char server[] = "6000-firebase-studio-1753223410587.cluster-73qgvk7hjjadkrjeyexca5ivva.cloudworkstations.dev";
const char endpoint[] = "/api/upload";
const int port = 443;
const char apn[] = "internet";


// --- Function Prototypes ---
void printModemStatus();
bool setupModem();
bool waitForNetwork();
bool manualGprsConnect();
bool sendFileInChunks(const char* filename);
bool sendChunk(const uint8_t* buffer, size_t size, size_t offset, size_t totalSize, const char* filename);
bool sendATCommand(const char* cmd, unsigned long timeout, const char* expectedResponse);
bool sendATCommand(const __FlashStringHelper* cmd, unsigned long timeout, const char* expectedResponse);


void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println(F("🔌 Booting..."));

    // Initialize SD card
    if (!SD.begin(SD_CS)) {
        Serial.println(F("❌ SD card initialization failed. Halting."));
        while (true);
    }
    Serial.println(F("✅ SD card ready."));

    // Initialize Modem
    if (!setupModem()) {
        Serial.println(F("❌ Modem initialization failed. Halting."));
        while (true);
    }

    // Connect to Network and GPRS
    if (!waitForNetwork() || !manualGprsConnect()) {
        Serial.println(F("❌ Network/GPRS connection failed. Halting."));
        while (true);
    }
    
    // Upload the file
    Serial.println(F("✅ Starting file upload process..."));
    if (sendFileInChunks("/sigma2.wav")) {
        Serial.println(F("✅ File upload completed successfully."));
    } else {
        Serial.println(F("❌ File upload failed."));
    }

    Serial.println(F("✅ Task finished. Entering idle loop."));
}

void loop() {
    // Keep the main loop clean. All work is done in setup().
    delay(10000);
}


bool setupModem() {
    Serial.println(F("Initializing modem..."));
    modemSerial.begin(MODEM_BAUD, SERIAL_8N1, MODEM_RX, MODEM_TX);

    Serial.println(F("Waiting for modem to be ready..."));
    int retry = 0;
    while(retry < 10) {
        if (modem.testAT(1000)) {
            Serial.println(F("✅ Modem is ready."));
            printModemStatus();
            return true;
        }
        Serial.print(".");
        retry++;
        delay(1000);
    }
    return false;
}

void printModemStatus() {
    Serial.println(F("--- Modem Status ---"));
    String imei = modem.getIMEI();
    if (imei.length() > 0) {
        Serial.print(F("IMEI: "));
        Serial.println(imei);
    }

    int signalQuality = modem.getSignalQuality();
    Serial.print(F("Signal Quality: "));
    Serial.println(signalQuality);

    int simStatus = modem.getSimStatus();
    Serial.print(F("SIM Status: "));
    Serial.println(simStatus);

    String ccid = modem.getSimCCID();
    if (ccid.length() > 0) {
        Serial.print(F("CCID: "));
        Serial.println(ccid);
    }

    String op = modem.getOperator();
     if (op.length() > 0) {
        Serial.print(F("Operator: "));
        Serial.println(op);
    }
    Serial.println(F("--------------------"));
}

bool waitForNetwork() {
    Serial.println(F("Waiting for network registration..."));
    unsigned long start = millis();
    while (millis() - start < 60000) { // 60-second timeout
        int regStatus = modem.getRegistrationStatus();
        Serial.print(F("Network registration status: "));
        Serial.println(regStatus);
        if (regStatus == 1 || regStatus == 5) {
            Serial.println(F("✅ Registered on network."));
            return true;
        }
        delay(2000);
    }
    Serial.println(F("❌ Network registration timed out."));
    return false;
}


bool manualGprsConnect() {
    Serial.println(F("Connecting to GPRS..."));

    // Set APN
    String cmd = "AT+CGDCONT=1,\"IP\",\"" + String(apn) + "\"";
    if (!sendATCommand(cmd.c_str(), 5000, "OK")) {
        Serial.println(F("Failed to set APN."));
        return false;
    }

    // Activate GPRS context
    if (!sendATCommand(F("AT+CGACT=1,1"), 20000, "OK")) {
        Serial.println(F("Failed to activate GPRS context."));
        return false;
    }
    
    // Check for IP address
    modem.sendAT(F("+CGPADDR=1"));
    String response = "";
    if (modem.waitResponse(10000, response) != 1) {
        Serial.println(F("Failed to get response for IP address check."));
        return false;
    }
    
    // This is a more robust way to find the IP address in the response.
    // It handles cases with extra whitespace or unexpected prefixes.
    const char* ipNeedle = "+CGPADDR: 1,";
    char* ipStart = strstr(response.c_str(), ipNeedle);
    if (ipStart != nullptr) {
        ipStart += strlen(ipNeedle); // Move pointer past the needle
        // Trim leading/trailing whitespace from the found IP
        String ipAddress = String(ipStart);
        ipAddress.trim();
        if (ipAddress.length() > 7) { // Basic sanity check for an IP
            Serial.print(F("✅ GPRS Connected. IP Address: "));
            Serial.println(ipAddress);
            return true;
        }
    }

    Serial.println(F("❌ Failed to verify GPRS connection and get IP."));
    return false;
}


bool sendFileInChunks(const char* filename) {
    File file = SD.open(filename);
    if (!file) {
        Serial.println(F("❌ Failed to open file for upload."));
        return false;
    }

    size_t totalSize = file.size();
    size_t offset = 0;

    Serial.print(F("Preparing to upload "));
    Serial.print(filename);
    Serial.print(F(" ("));
    Serial.print(totalSize);
    Serial.println(F(" bytes)"));

    while (offset < totalSize) {
        uint8_t buffer[CHUNK_SIZE];
        size_t chunkSize = min((size_t)CHUNK_SIZE, totalSize - offset);

        file.seek(offset);
        size_t bytesRead = file.read(buffer, chunkSize);

        if (bytesRead != chunkSize) {
            Serial.println(F("❌ SD card read error. Aborting."));
            file.close();
            return false;
        }

        bool success = false;
        for (int retry = 0; retry < 3; retry++) {
            Serial.print(F("  Attempt "));
            Serial.print(retry + 1);
            Serial.print(F("/3 to send chunk at offset "));
            Serial.print(offset);
            Serial.println("...");

            if (sendChunk(buffer, chunkSize, offset, totalSize, filename)) {
                success = true;
                break;
            }
            Serial.println(F("    ...chunk failed. Retrying in 3 seconds."));
            delay(3000);
        }

        if (!success) {
            Serial.print(F("❌ Failed to upload chunk at offset "));
            Serial.print(offset);
            Serial.println(F(" after 3 retries. Aborting."));
            file.close();
            return false;
        }

        offset += chunkSize;
    }

    file.close();
    return true;
}

bool sendChunk(const uint8_t* buffer, size_t size, size_t offset, size_t totalSize, const char* filename) {
    // 1. Start HTTPS Service
    if (!sendATCommand(F("AT+CHTTPSSTART"), 10000, "OK")) {
        sendATCommand(F("AT+CHTTPSSTOP"), 5000, "OK"); // Attempt cleanup
        return false;
    }
    
    // 2. Open HTTPS Session
    String cmd = "AT+CHTTPSOPSE=\"" + String(server) + "\"," + String(port);
    if (!sendATCommand(cmd.c_str(), 20000, "+CHTTPSOPSE: 0")) {
        sendATCommand(F("AT+CHTTPSCLSE"), 5000, "OK"); // Attempt cleanup
        sendATCommand(F("AT+CHTTPSSTOP"), 5000, "OK");
        return false;
    }

    // 3. Set request headers
    String headers = "POST " + String(endpoint) + " HTTP/1.1\r\n" +
                     "Host: " + String(server) + "\r\n" +
                     "X-Filename: " + String(filename) + "\r\n" +
                     "X-Chunk-Offset: " + String(offset) + "\r\n" +
                     "X-Chunk-Size: " + String(size) + "\r\n" +
                     "X-Total-Size: " + String(totalSize) + "\r\n" +
                     "Content-Type: application/octet-stream\r\n" +
                     "Content-Length: " + String(size) + "\r\n" +
                     "Connection: close\r\n\r\n";

    cmd = "AT+CHTTPSSEND=" + String(headers.length());
    if (!sendATCommand(cmd.c_str(), 5000, ">")) {
        sendATCommand(F("AT+CHTTPSCLSE"), 5000, "OK");
        sendATCommand(F("AT+CHTTPSSTOP"), 5000, "OK");
        return false;
    }
    
    modem.stream.print(headers);
    modem.stream.flush();
    if(modem.waitResponse(10000) != 1) { // Wait for OK after sending headers
       Serial.println("Failed to get OK after headers");
    }

    // 4. Send chunk data
    cmd = "AT+CHTTPSSEND=" + String(size);
    if (!sendATCommand(cmd.c_str(), 5000, ">")) {
        sendATCommand(F("AT+CHTTPSCLSE"), 5000, "OK");
        sendATCommand(F("AT+CHTTPSSTOP"), 5000, "OK");
        return false;
    }

    modem.stream.write(buffer, size);
    modem.stream.flush();
    if(modem.waitResponse(10000) != 1) {
       Serial.println("Failed to get OK after body");
    }
    
    // 5. Read response
    String response;
    bool success = false;
    unsigned long start = millis();
    while (millis() - start < 20000) {
        if (modem.stream.available()) {
            String line = modem.stream.readStringUntil('\n');
            line.trim();
            if (line.startsWith("HTTP/1.1 200")) {
                Serial.println(F("✅ Chunk uploaded successfully (HTTP 200 OK)."));
                success = true;
            } else if (line.length() > 0) {
                // To avoid spamming, only print non-empty lines
                Serial.print("[SERVER] ");
                Serial.println(line);
            }
        }
        if (success) break; // Exit loop once we have a 200 OK
    }

    // 6. Cleanup
    sendATCommand(F("AT+CHTTPSCLSE"), 5000, "OK");
    sendATCommand(F("AT+CHTTPSSTOP"), 5000, "OK");

    return success;
}

bool sendATCommand(const char* cmd, unsigned long timeout, const char* expectedResponse) {
    modem.sendAT(cmd);
    if (modem.waitResponse(timeout, expectedResponse) != 1) {
        TINY_GSM_DEBUG_PRINT(F("[DEBUG] Timeout or unexpected response for: "));
        TINY_GSM_DEBUG_PRINTLN(cmd);
        String response;
        modem.stream.readString(response); // Read whatever is in the buffer
        TINY_GSM_DEBUG_PRINT(F("[DEBUG] Received: "));
        TINY_GSM_DEBUG_PRINTLN(response);
        return false;
    }
    return true;
}

// Overload for Flash String Helper
bool sendATCommand(const __FlashStringHelper* cmd, unsigned long timeout, const char* expectedResponse) {
    modem.sendAT(cmd);
    if (modem.waitResponse(timeout, expectedResponse) != 1) {
        TINY_GSM_DEBUG_PRINT(F("[DEBUG] Timeout or unexpected response for: "));
        TINY_GSM_DEBUG_PRINTLN(cmd);
        String response;
        modem.stream.readString(response);
        TINY_GSM_DEBUG_PRINT(F("[DEBUG] Received: "));
        TINY_GSM_DEBUG_PRINTLN(response);
        return false;
    }
    return true;
}
