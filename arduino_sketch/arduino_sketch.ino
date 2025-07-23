
#include <Arduino.h>
#include <HardwareSerial.h>
#include <SD.h>
#include <SPI.h>

// Correct pins for your ESP32 board
#define SD_CS_PIN 5
#define SPI_MOSI_PIN 23
#define SPI_MISO_PIN 19
#define SPI_SCK_PIN 18

#define MODEM_RX_PIN 16
#define MODEM_TX_PIN 17
#define MODEM_BAUD 115200

HardwareSerial modemSerial(1);

#define TINY_GSM_MODEM_SIM7600
#include <TinyGsmClient.h>

// Your GPRS credentials
const char apn[] = "internet"; // APN for One NZ
const char user[] = "";
const char pass[] = "";

// Server details
const char server[] = "6000-firebase-studio-1753223410587.cluster-73qgvk7hjjadkrjeyexca5ivva.cloudworkstations.dev";
const int serverPort = 443;

TinyGsm modem(modemSerial);

const size_t CHUNK_BUFFER_SIZE = 4096;

// Function Prototypes
void setupModem();
bool sendChunk(fs::File& file, const char* filename, size_t fileSize, size_t offset);
void sendFileChunks(const char* filename);
bool waitForModemReady();
bool waitForNetwork();
bool openHttpsSession();
void closeHttpsSession();

// Wrapper for sending AT commands and waiting for a response
String sendAT(String cmd, unsigned long timeout = 10000) {
    Serial.print("[AT SEND] ");
    Serial.println(cmd);
    modemSerial.println(cmd);
    String res;
    modem.waitResponse(timeout, res);
    Serial.println(res);
    return res;
}

// Wrapper to send an AT command and check for an expected response
bool sendATCheck(String cmd, String expected, unsigned long timeout = 10000) {
    String res = sendAT(cmd, timeout);
    return res.indexOf(expected) != -1;
}

void setup() {
    Serial.begin(115200);
    while (!Serial);

    Serial.println("🔌 Booting...");

    SPI.begin(SPI_SCK_PIN, SPI_MISO_PIN, SPI_MOSI_PIN, SD_CS_PIN);
    if (!SD.begin(SD_CS_PIN)) {
        Serial.println("❌ SD Card Mount Failed");
        return;
    }
    Serial.println("✅ SD card ready.");
    
    setupModem();

    // List of files to upload
    const char* filesToUpload[] = {"/sigma2.wav"};
    for (const char* filename : filesToUpload) {
        sendFileChunks(filename);
    }
}

void loop() {
    // Everything is done in setup for this example
}

void setupModem() {
    Serial.println("Initializing modem...");
    modemSerial.begin(MODEM_BAUD, SERIAL_8N1, MODEM_RX_PIN, MODEM_TX_PIN);

    if (!waitForModemReady()) {
        Serial.println("❌ Modem not responding.");
        return;
    }

    Serial.println("✅ Modem and SIM are ready.");

    Serial.println("--- Modem Status ---");
    String imei = modem.getIMEI();
    imei.replace("AT+CIMI\r\n", ""); // Clean up response
    Serial.println("IMEI: " + imei);

    Serial.print("Signal Quality: ");
    Serial.println(modem.getSignalQuality());
    Serial.print("SIM Status: ");
    Serial.println(modem.getSimStatus());
    Serial.print("CCID: ");
    Serial.println(modem.getSimCCID());
    Serial.print("Operator: ");
    Serial.println(modem.getOperator());
    Serial.println("--------------------");

    Serial.println("📡 Connecting to network...");
    if (!waitForNetwork()) {
        Serial.println("❌ Network registration failed.");
        return;
    }
    
    if (!modem.gprsConnect(apn, user, pass)) {
        Serial.println("❌ GPRS connection failed.");
        return;
    }
    
    Serial.println("✅ GPRS connected.");
    String ip = modem.getLocalIP();
    ip.replace("AT+IPADDR\r\n", ""); // clean up
    Serial.println("Local IP: " + ip);
}

bool waitForModemReady() {
    for (int i = 0; i < 30; i++) { // Wait up to 15 seconds
        if (modem.testAT(1000)) {
            SimStatus sim = modem.getSimStatus();
            if (sim == SIM_READY) {
                return true;
            }
        }
        delay(500);
    }
    return false;
}

bool waitForNetwork() {
    Serial.println("Waiting for network registration...");
    for (int i = 0; i < 30; i++) { // Wait up to 30 seconds
        NetworkStatus reg = modem.getRegistrationStatus();
        Serial.printf("Network registration status: %d\n", reg);
        if (reg == REG_REGISTERED_HOME || reg == REG_REGISTERED_ROAMING) {
            Serial.println("✅ Registered on network.");
            return true;
        }
        delay(1000);
    }
    return false;
}

bool openHttpsSession() {
    if (!sendATCheck("AT+CHTTPSSTART", "OK")) return false;
    
    String cmd = "AT+CHTTPSOPSE=\"";
    cmd += server;
    cmd += "\",";
    cmd += serverPort;
    
    String res;
    modemSerial.println(cmd);
    unsigned long start = millis();
    bool found = false;
    while(millis() - start < 20000) { // 20 second timeout
        if (modemSerial.available()) {
            String line = modemSerial.readStringUntil('\n');
            res += line;
            if (line.indexOf("+CHTTPSOPSE: 0") != -1) {
                found = true;
                break;
            }
        }
    }
    
    if (!found) {
        Serial.println("[DEBUG] Timeout waiting for: +CHTTPSOPSE: 0");
        Serial.println("[DEBUG] Received: " + res);
        return false;
    }
    
    return true;
}

void closeHttpsSession() {
    sendATCheck("AT+CHTTPSCLSE", "OK");
    sendATCheck("AT+CHTTPSSTOP", "OK");
}

void sendFileChunks(const char* filename) {
    File file = SD.open(filename, FILE_READ);
    if (!file) {
        Serial.printf("❌ Failed to open file: %s\n", filename);
        return;
    }

    size_t fileSize = file.size();
    Serial.printf("📤 Preparing to upload %s (%d bytes)\n", filename, fileSize);

    size_t offset = 0;
    while (offset < fileSize) {
        if (!sendChunk(file, filename, fileSize, offset)) {
            Serial.printf("❌ File upload failed.\n");
            file.close();
            return;
        }
        offset = file.position(); // Update offset to the new position
    }

    Serial.println("✅ File upload complete.");
    file.close();
}


bool sendChunk(fs::File& file, const char* filename, size_t fileSize, size_t offset) {
    if (!openHttpsSession()) {
        Serial.println("❌ Failed to open HTTPS session with server.");
        closeHttpsSession();
        return false;
    }

    uint8_t chunkBuffer[CHUNK_BUFFER_SIZE];
    size_t chunkSize = file.read(chunkBuffer, CHUNK_BUFFER_SIZE);
    
    if (chunkSize == 0) return true; // End of file

    // Construct headers
    String headers = "x-filename: " + String(filename) + "\r\n";
    headers += "x-chunk-offset: " + String(offset) + "\r\n";
    headers += "x-chunk-size: " + String(chunkSize) + "\r\n";
    headers += "x-total-size: " + String(fileSize) + "\r\n";
    headers += "Content-Type: application/octet-stream\r\n";
    
    String cmd = "AT+CHTTPSPOST=\"/api/upload\",";
    cmd += headers.length();
    cmd += ",";
    cmd += chunkSize;
    cmd += ",10000"; // 10 second timeout for content

    modemSerial.println(cmd);

    unsigned long start = millis();
    bool readyForData = false;
    while(millis() - start < 5000) { // 5 sec timeout to get ">"
        if (modemSerial.available()) {
            if (modemSerial.read() == '>') {
                readyForData = true;
                break;
            }
        }
    }

    if (!readyForData) {
        Serial.println("[DEBUG] Timeout waiting for: >");
        String res;
        modem.waitResponse(100, res);
        Serial.println("[DEBUG] Received: " + res);
        Serial.println("❌ Modem did not respond to POST command. Aborting.");
        closeHttpsSession();
        return false;
    }

    // Send headers
    modemSerial.print(headers);
    // Send binary data
    modemSerial.write(chunkBuffer, chunkSize);
    
    // Wait for response
    String res;
    modem.waitResponse(20000, res); // 20 sec timeout for server response
    
    if (res.indexOf("+CHTTPS: POST,200") == -1 && res.indexOf("+CHTTPS: POST,201") == -1) {
        Serial.println("❌ Server returned an error:");
        Serial.println(res);
        closeHttpsSession();
        return false;
    }

    Serial.printf("✅ Sent chunk. Offset: %d, Size: %d\n", offset, chunkSize);
    closeHttpsSession();
    return true;
}
