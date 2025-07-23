// Defines the serial port for the SIM7600 module
#define SerialAT Serial1

// Defines the serial baud rate for the SIM7600 module
#define TINY_GSM_MODEM_SIM7600
#define TINY_GSM_USE_GPRS true
#define TINY_GSM_USE_SSL true

// Required libraries
#include <TinyGsmClient.h>
#include <SPI.h>
#include <SD.h>

// Server details
const char server[] = "6000-firebase-studio-1753223410587.cluster-73qgvk7hjjadkrjeyexca5ivva.cloudworkstations.dev";
const char resource[] = "/api/upload";
const int port = 443;

// APN details for your mobile network provider
const char apn[] = "internet"; 
const char gprsUser[] = "";
const char gprsPass[] = "";

// Pins for UART communication with the SIM7600 module
#define UART_TX 2
#define UART_RX 4

// Pin for the SD card chip select
#define SD_CS 5

// Global objects
TinyGsm modem(SerialAT);
TinyGsmClientSecure client(modem);

void setup() {
    // Start serial communication for debugging
    Serial.begin(115200);
    delay(10);

    // Start serial communication with the SIM7600 module
    SerialAT.begin(115200, SERIAL_8N1, UART_RX, UART_TX);
    delay(6000);

    Serial.println("? Initializing modem...");
    if (!modem.init()) {
        Serial.println("? Failed to initialize modem. Halting.");
        while (true);
    }
    
    // Set modem to full functionality
    modem.sendAT(F("+CFUN=1"));
    if (modem.waitResponse(10000L) != 1) {
        Serial.println(F("? CFUN command failed"));
    }

    // Set modem to LTE-only mode for 4G
    modem.sendAT(F("+CNMP=38"));
    if (modem.waitResponse(10000L) != 1) {
        Serial.println(F("? CNMP command failed"));
    }

    Serial.print("? Waiting for network...");
    if (!modem.waitForNetwork()) {
        Serial.println(" fail");
        delay(10000);
        return;
    }
    Serial.println(" OK");

    Serial.print("? Connecting to APN: ");
    Serial.print(apn);
    if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
        Serial.println(" fail");
        delay(10000);
        return;
    }
    Serial.println(" OK");

    // Initialize the SD card
    Serial.print(F("? Initializing SD card..."));
    if (!SD.begin(SD_CS)) {
        Serial.println(F(" failed!"));
        while (true);
    }
    Serial.println(F(" OK"));

    // The file to upload
    const char* filename = "/sigma2.wav";
    uploadFile(filename);
}

void loop() {
    // Everything is done in setup, so the loop is empty.
}

void uploadFile(const char *filename) {
    File file = SD.open(filename, FILE_READ);
    if (!file) {
        Serial.println(F("? Failed to open file for reading"));
        return;
    }

    size_t fileSize = file.size();
    Serial.print(F("? Preparing to upload "));
    Serial.print(filename);
    Serial.print(F(" ("));
    Serial.print(fileSize);
    Serial.println(F(" bytes)"));

    const size_t chunkSize = 1024 * 10; // 10KB chunks
    size_t offset = 0;

    while (offset < fileSize) {
        Serial.print(F("  Connecting to server... "));
        if (!client.connect(server, port)) {
            Serial.println(F("failed. Retrying..."));
            delay(5000);
            continue;
        }
        Serial.println("OK");

        size_t currentChunkSize = min(chunkSize, fileSize - offset);

        // Construct HTTP POST request
        String headers = "POST " + String(resource) + " HTTP/1.1\r\n";
        headers += "Host: " + String(server) + "\r\n";
        headers += "Connection: close\r\n";
        headers += "Content-Type: application/octet-stream\r\n";
        headers += "X-Filename: " + String(filename).substring(1) + "\r\n";
        headers += "X-Chunk-Offset: " + String(offset) + "\r\n";
        headers += "X-Chunk-Size: " + String(currentChunkSize) + "\r\n";
        headers += "X-Total-Size: " + String(fileSize) + "\r\n";
        headers += "Content-Length: " + String(currentChunkSize) + "\r\n";
        headers += "\r\n";
        
        Serial.print(F("  Sending chunk at offset "));
        Serial.print(offset);
        Serial.print(" (");
        Serial.print(currentChunkSize);
        Serial.println(" bytes)");

        // Send headers
        client.print(headers);
        
        // Send file data
        uint8_t buffer[256];
        size_t bytesSent = 0;
        file.seek(offset);

        while (bytesSent < currentChunkSize) {
            size_t toRead = min(sizeof(buffer), currentChunkSize - bytesSent);
            size_t bytesRead = file.read(buffer, toRead);
            if (bytesRead == 0) break;
            
            size_t written = client.write(buffer, bytesRead);
            if (written != bytesRead) {
                Serial.println(F("? Failed to write to client."));
                break;
            }
            bytesSent += bytesRead;
        }

        // Wait for response from server
        unsigned long timeout = millis();
        while (client.connected() && !client.available() && millis() - timeout < 30000L) {
            delay(100);
        }

        if (client.available()) {
            String line = client.readStringUntil('\n');
            if (line.indexOf("200 OK") != -1) {
                Serial.println(F("  Chunk uploaded successfully."));
                offset += currentChunkSize;
            } else {
                 Serial.print(F("  Server returned an error: "));
                 Serial.println(line);
                 while(client.available()) {
                    Serial.write(client.read());
                 }
            }
        } else {
            Serial.println(F("  No response from server."));
        }
        
        client.stop();
        Serial.println(F("  Connection closed."));
        delay(1000); // Small delay between chunks
    }

    file.close();
    Serial.println(F("? File upload complete."));
}
