
// LIBRARIES
#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <TinyGsmClient.h>

// DEFINITIONS
#define SerialAT Serial1
#define TINY_GSM_MODEM_SIM7600
#define CHUNK_SIZE 1024 * 32 // 32KB

// PINOUT
#define TX_PIN 26
#define RX_PIN 27
#define SD_CS 5

// SERVER CONFIG
const char server[] = "6000-firebase-studio-1753223410587.cluster-73qgvk7hjjadkrjeyexca5ivva.cloudworkstations.dev";
const char resource[] = "/api/upload";
const int port = 443;

// MODEM & FILE CONFIG
TinyGsm modem(SerialAT);
TinyGsmClientSecure client(modem);
File root;
const char *upload_filename = "/sigma2.wav";

// HELPERS
void printTimestamp() {
    char timestamp[20];
    sprintf(timestamp, "%02d:%02d:%02d.%03d -> ", hour(), minute(), second(), millis() % 1000);
    Serial.print(timestamp);
}

void setupModem() {
    printTimestamp();
    Serial.println("? Initializing modem...");
    SerialAT.begin(115200, SERIAL_8N1, RX_PIN, TX_PIN);
    delay(3000);
    modem.init();

    printTimestamp();
    Serial.println("? Setting modem to 4G/LTE mode...");
    if (!modem.setNetworkMode(38)) { // 38 is LTE only
        printTimestamp();
        Serial.println("! Failed to set network mode to LTE.");
    }
    
    printTimestamp();
    Serial.println("? Setting modem to full functionality...");
    if (!modem.setFunctionality(1)) {
        printTimestamp();
        Serial.println("! Failed to set modem to full functionality.");
    }

    printTimestamp();
    Serial.println("? Modem initialized.");
}

void setup() {
    Serial.begin(115200);
    while (!Serial);

    printTimestamp();
    Serial.println("? Booting...");

    if (!SD.begin(SD_CS)) {
        printTimestamp();
        Serial.println("! SD card initialization failed.");
        return;
    }
    printTimestamp();
    Serial.println("? SD card ready.");
    
    setupModem();
}

void uploadFile(const char *filename) {
    File file = SD.open(filename);
    if (!file) {
        printTimestamp();
        Serial.print("! Failed to open file: ");
        Serial.println(filename);
        return;
    }

    size_t fileSize = file.size();
    printTimestamp();
    Serial.print("? Preparing to upload ");
    Serial.print(filename);
    Serial.print(" (");
    Serial.print(fileSize);
    Serial.println(" bytes)");

    size_t offset = 0;
    while (offset < fileSize) {
        size_t chunkSize = min((size_t)CHUNK_SIZE, fileSize - offset);
        
        printTimestamp();
        Serial.print("  Connecting to ");
        Serial.print(server);
        Serial.println("...");
        if (!client.connect(server, port)) {
            printTimestamp();
            Serial.println("! Connection failed.");
            return;
        }

        printTimestamp();
        Serial.print("  Uploading chunk at offset ");
        Serial.print(offset);
        Serial.print(" (");
        Serial.print(chunkSize);
        Serial.println(" bytes)...");

        // Make a HTTP request
        String request = "POST " + String(resource) + " HTTP/1.1\r\n";
        request += "Host: " + String(server) + "\r\n";
        request += "Connection: close\r\n";
        request += "X-Filename: " + String(filename).substring(1) + "\r\n";
        request += "X-Total-Size: " + String(fileSize) + "\r\n";
        request += "X-Chunk-Offset: " + String(offset) + "\r\n";
        request += "X-Chunk-Size: " + String(chunkSize) + "\r\n";
        request += "Content-Type: application/octet-stream\r\n";
        request += "Content-Length: " + String(chunkSize) + "\r\n";
        request += "\r\n";
        
        client.print(request);

        // Write the chunk data
        uint8_t buffer[256];
        size_t bytesSent = 0;
        while (bytesSent < chunkSize) {
            size_t bytesToRead = min((size_t)sizeof(buffer), chunkSize - bytesSent);
            size_t bytesRead = file.read(buffer, bytesToRead);
            if (bytesRead > 0) {
                client.write(buffer, bytesRead);
                bytesSent += bytesRead;
            } else {
                break;
            }
        }
        
        printTimestamp();
        Serial.println("  Chunk sent. Waiting for response...");

        // Read response
        unsigned long timeout = millis();
        while (client.connected() && millis() - timeout < 10000L) {
            if (client.available()) {
                String line = client.readStringUntil('\n');
                // You can print the response here if needed for debugging
                // Serial.println(line);
                if (line.startsWith("HTTP/1.1 200 OK")) {
                   printTimestamp();
                   Serial.println("? Chunk uploaded successfully.");
                }
            }
        }

        client.stop();
        printTimestamp();
        Serial.println("  Connection closed.");

        offset += chunkSize;
    }

    file.close();
    printTimestamp();
    Serial.println("? File upload process complete.");
}


void loop() {
    printTimestamp();
    Serial.println("? Waiting for network registration...");
    if (!modem.waitForNetwork()) {
        printTimestamp();
        Serial.println("! Failed to register on network.");
        delay(10000);
        return;
    }
    printTimestamp();
    Serial.println("? Registered on network.");

    printTimestamp();
    Serial.println("? Connecting to GPRS...");
    if (!modem.gprsConnect("internet")) {
        printTimestamp();
        Serial.println("! Failed to connect to GPRS.");
        delay(10000);
        return;
    }

    printTimestamp();
    Serial.print("? GPRS Connected. IP: ");
    Serial.println(modem.getLocalIP());

    uploadFile(upload_filename);

    printTimestamp();
    Serial.println("? Disconnecting GPRS...");
    modem.gprsDisconnect();
    printTimestamp();
    Serial.println("? GPRS Disconnected.");

    // Wait for a long time before next cycle
    printTimestamp();
    Serial.println("? Entering deep sleep for 1 hour.");
    delay(3600000);
}
