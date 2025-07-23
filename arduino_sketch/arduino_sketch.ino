// Define the modem model
#define TINY_GSM_MODEM_SIM7600

// Increase the default buffer size for TinyGSM
#define TINY_GSM_RX_BUFFER 512

// Required libraries
#include <HardwareSerial.h>
#include <SD.h>
#include <SPI.h>

// --- Pin Definitions ---
#define MODEM_TX 17
#define MODEM_RX 16
#define SD_CS 5

// --- Modem & Server Configuration ---
#define MODEM_BAUD 115200
const char server[] = "6000-firebase-studio-1753223410587.cluster-73qgvk7hjjadkrjeyexca5ivva.cloudworkstations.dev";
const char resource[] = "/api/upload";
const int port = 443;
const char apn[] = "internet"; // Use "internet" for One NZ

// --- File Upload Configuration ---
const char* upload_filename = "/sigma2.wav";
#define CHUNK_SIZE 4096 // Use a larger chunk size for better efficiency

// --- Global Objects ---
HardwareSerial XCOM(1); // Use UART1 for the modem

// --- Function Prototypes ---
// (Forward declarations to prevent 'not declared in this scope' errors)
bool sendATCommand(const char* cmd, unsigned long timeout, const char* expect1, const char* expect2 = nullptr, String* response = nullptr);
bool sendATCommand(const __FlashStringHelper* cmd, unsigned long timeout, const char* expect1, const char* expect2 = nullptr, String* response = nullptr);

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println(F("? Booting..."));

    if (!SD.begin(SD_CS)) {
        Serial.println(F("? SD card failed. Halting."));
        while (true);
    }
    Serial.println(F("? SD card ready."));

    XCOM.begin(MODEM_BAUD, SERIAL_8N1, MODEM_RX, MODEM_TX);

    initializeModem();
    printModemStatus();

    if (!waitForNetwork()) {
        Serial.println(F("? Network registration failed. Halting."));
        while (true);
    }

    if (!manualGprsConnect()) {
        Serial.println(F("? GPRS connection failed. Halting."));
        while (true);
    }

    Serial.printf("? Preparing to upload %s (%d bytes)\n", upload_filename, SD.open(upload_filename).size());
    uploadFileInChunks(upload_filename);
}

void loop() {
    // Keep the main loop clean. All work is done in setup.
}

// --- Core Functions ---

void initializeModem() {
    Serial.println(F("? Initializing modem..."));
    int attempts = 0;
    while (attempts < 5) {
        if (sendATCommand(F("AT"), 1000, "OK")) {
            Serial.println(F("? Modem is ready."));
            return;
        }
        attempts++;
        delay(1000);
    }
    Serial.println(F("? Modem not responding. Halting."));
    while(true);
}

bool waitForNetwork() {
    Serial.println(F("? Waiting for network registration..."));
    int attempts = 0;
    while (attempts < 30) {
        String response;
        sendATCommand(F("AT+CREG?"), 1000, "OK", nullptr, &response);
        if (response.indexOf("+CREG: 0,1") != -1 || response.indexOf("+CREG: 0,5") != -1) {
            Serial.println(F("? Registered on network."));
            return true;
        }
        attempts++;
        delay(2000);
    }
    return false;
}

bool manualGprsConnect() {
    Serial.println(F("? Connecting to GPRS..."));
    String cmd = "AT+CGDCONT=1,\"IP\",\"" + String(apn) + "\"";
    if (!sendATCommand(cmd.c_str(), 5000, "OK")) {
        Serial.println(F("? Failed to set APN."));
        return false;
    }
    if (!sendATCommand(F("AT+CGACT=1,1"), 5000, "OK")) {
        Serial.println(F("? Failed to activate GPRS context."));
        return false;
    }

    String response;
    if (sendATCommand(F("AT+CGPADDR=1"), 10000, "OK", nullptr, &response)) {
        int idx = response.indexOf("+CGPADDR: 1,");
        if (idx != -1) {
            String ip = response.substring(idx + 12);
            ip.trim();
            Serial.println("? GPRS Connected. IP: " + ip);
            return true;
        }
    }
    return false;
}

bool openHttpsSession() {
    if (!sendATCommand(F("AT+CHTTPSSTART"), 20000, "OK")) {
        Serial.println(F("? Failed to start HTTPS service."));
        return false;
    }

    String cmd_url = "AT+CHTTPSPARA=\"URL\",\"" + String(resource) + "\"";
    if (!sendATCommand(cmd_url.c_str(), 30000, "OK")) {
        Serial.println(F("? Failed to set URL parameter."));
        return false;
    }
    
    String cmd_opse = "AT+CHTTPSOPSE=\"" + String(server) + "\"," + String(port);
    if (!sendATCommand(cmd_opse.c_str(), 30000, "+CHTTPSOPSE: 0")) {
         Serial.println(F("? Failed to open HTTPS session."));
         return false;
    }

    return true;
}

bool setRequestHeaders(const char* filename, size_t offset, size_t chunkSize, size_t totalSize) {
    String header_cmd;
    header_cmd = "AT+CHTTPSPARA=\"USERDATA\",\"Content-Type: application/octet-stream\"";
    if (!sendATCommand(header_cmd.c_str(), 5000, "OK")) return false;

    header_cmd = "AT+CHTTPSPARA=\"USERDATA\",\"X-Filename: " + String(filename) + "\"";
    if (!sendATCommand(header_cmd.c_str(), 5000, "OK")) return false;
    
    header_cmd = "AT+CHTTPSPARA=\"USERDATA\",\"X-Chunk-Offset: " + String(offset) + "\"";
    if (!sendATCommand(header_cmd.c_str(), 5000, "OK")) return false;

    header_cmd = "AT+CHTTPSPARA=\"USERDATA\",\"X-Chunk-Size: " + String(chunkSize) + "\"";
    if (!sendATCommand(header_cmd.c_str(), 5000, "OK")) return false;

    header_cmd = "AT+CHTTPSPARA=\"USERDATA\",\"X-Total-Size: " + String(totalSize) + "\"";
    if (!sendATCommand(header_cmd.c_str(), 5000, "OK")) return false;
    
    header_cmd = "AT+CHTTPSPARA=\"USERDATA\",\"Content-Length: " + String(chunkSize) + "\"";
    if (!sendATCommand(header_cmd.c_str(), 5000, "OK")) return false;

    return true;
}

void closeHttpsSession() {
    sendATCommand(F("AT+CHTTPSCLSE"), 10000, "OK");
    sendATCommand(F("AT+CHTTPSSTOP"), 10000, "OK");
}

void uploadFileInChunks(const char* filename) {
    File file = SD.open(filename);
    if (!file) {
        Serial.println(F("? Failed to open file for upload."));
        return;
    }

    size_t totalSize = file.size();
    size_t offset = 0;

    while (offset < totalSize) {
        bool chunkSuccess = false;
        for (int attempt = 1; attempt <= 3; ++attempt) {
            Serial.printf("  Attempt %d/3 to send chunk at offset %u...\n", attempt, offset);
            
            if (openHttpsSession()) {
                size_t chunkSize = min((size_t)CHUNK_SIZE, totalSize - offset);
                if (setRequestHeaders(filename, offset, chunkSize, totalSize)) {
                    if (sendChunk(file, offset, chunkSize)) {
                        chunkSuccess = true;
                    }
                }
            }
            closeHttpsSession(); // Always close session after attempt
            
            if (chunkSuccess) {
                break; // Exit retry loop if successful
            }
            Serial.println(F("? Retrying in 5 seconds..."));
            delay(5000);
        }

        if (chunkSuccess) {
            offset += CHUNK_SIZE;
        } else {
            Serial.println(F("? Failed to upload chunk after 3 attempts. Aborting."));
            break;
        }
    }

    file.close();
    if (offset >= totalSize) {
        Serial.println(F("? File upload completed successfully."));
    }
}

bool sendChunk(File& file, size_t offset, size_t chunkSize) {
    String cmd = "AT+CHTTPSSEND=" + String(chunkSize);
    if (!sendATCommand(cmd.c_str(), 5000, ">")) {
        Serial.println(F("? Failed to initiate chunk send."));
        return false;
    }

    uint8_t buffer[256];
    file.seek(offset);
    size_t bytesSent = 0;
    while(bytesSent < chunkSize) {
        size_t toRead = min((size_t)sizeof(buffer), chunkSize - bytesSent);
        size_t bytesRead = file.read(buffer, toRead);
        XCOM.write(buffer, bytesRead);
        bytesSent += bytesRead;
    }

    String response;
    if (sendATCommand(static_cast<const char*>(nullptr), 30000, "+CHTTPSSEND: 0", "OK", &response)) {
        if (response.indexOf("200 OK") != -1) {
            Serial.printf("? Chunk at offset %u sent successfully.\n", offset);
            return true;
        }
    }
    
    Serial.println(F("? Chunk upload failed."));
    return false;
}

// --- Utility Functions ---

void printModemStatus() {
    Serial.println(F("--- Modem Status ---"));
    String response;

    sendATCommand(F("AT+GSN"), 1000, "OK", nullptr, &response);
    int imei_start = response.indexOf("\r\n");
    if(imei_start != -1) {
        String imei = response.substring(imei_start + 2);
        imei.trim();
        Serial.println("IMEI: " + imei);
    }
    
    response = "";
    sendATCommand(F("AT+CSQ"), 1000, "OK", nullptr, &response);
    if (response.indexOf("+CSQ:") != -1) {
        Serial.println("Signal Quality: " + response.substring(response.indexOf("+CSQ:") + 6, response.indexOf(",")));
    }
    
    response = "";
    sendATCommand(F("AT+CPIN?"), 1000, "OK", nullptr, &response);
    if (response.indexOf("READY") != -1) {
        Serial.println("SIM Status: Ready");
    }

    response = "";
    sendATCommand(F("AT+CCID"), 1000, "OK", nullptr, &response);
    if (response.indexOf("+CCID:") != -1) {
        String ccid = response.substring(response.indexOf("+CCID:") + 7);
        ccid.trim();
        Serial.println("CCID: " + ccid);
    }

    response = "";
    sendATCommand(F("AT+COPS?"), 5000, "OK", nullptr, &response);
    if (response.indexOf("+COPS:") != -1) {
        int first_quote = response.indexOf("\"");
        int second_quote = response.indexOf("\"", first_quote + 1);
        if (first_quote != -1 && second_quote != -1) {
            Serial.println("Operator: " + response.substring(first_quote + 1, second_quote));
        }
    }
    Serial.println(F("--------------------"));
}

bool sendATCommand(const char* cmd, unsigned long timeout, const char* expect1, const char* expect2, String* response) {
    if (cmd) {
        XCOM.println(cmd);
        Serial.print(F("[AT SEND] ")); Serial.println(cmd);
    }

    unsigned long start = millis();
    String res;
    while (millis() - start < timeout) {
        while (XCOM.available()) {
            char c = XCOM.read();
            res += c;
        }
        if (res.indexOf(expect1) != -1) {
            if (!expect2 || (expect2 && res.indexOf(expect2) != -1)) {
                if (response) *response = res;
                Serial.print(F("[AT RECV] ")); Serial.println(res);
                return true;
            }
        }
    }
    Serial.print(F("[AT RECV TIMEOUT] ")); Serial.println(res);
    return false;
}

bool sendATCommand(const __FlashStringHelper* cmd, unsigned long timeout, const char* expect1, const char* expect2, String* response) {
    if (cmd) {
        XCOM.println(cmd);
        Serial.print(F("[AT SEND] ")); Serial.println(cmd);
    }
    
    unsigned long start = millis();
    String res;
    while (millis() - start < timeout) {
        while (XCOM.available()) {
            char c = XCOM.read();
            res += c;
        }
        if (res.indexOf(expect1) != -1) {
            if (!expect2 || (expect2 && res.indexOf(expect2) != -1)) {
                if (response) *response = res;
                Serial.print(F("[AT RECV] ")); Serial.println(res);
                return true;
            }
        }
    }
    Serial.print(F("[AT RECV TIMEOUT] ")); Serial.println(res);
    return false;
}
