
#define TINY_GSM_MODEM_SIM7600

#include <Arduino.h>
#include <HardwareSerial.h>
#include <TinyGsmClient.h>
#include <SSLClient.h>
#include <SPI.h>
#include <SD.h>

// Modem pins
#define UART_BAUD 115200
#define PIN_TX 27
#define PIN_RX 26

// SD card pins
#define SD_MISO 2
#define SD_MOSI 15
#define SD_SCLK 14
#define SD_CS 13

#define CHUNK_SIZE (128 * 1024) // 128KB chunks

const char* server = "6000-firebase-studio-1753223410587.cluster-73qgvk7hjjadkrjeyexca5ivva.cloudworkstations.dev";
const int port = 443;
const char* resource = "/api/upload";
const char* apn = "internet";

HardwareSerial SerialAT(1);
TinyGsm modem(SerialAT);
TinyGsmClient base_client(modem);
SSLClient client(&base_client);

void setup() {
    Serial.begin(115200);
    while (!Serial);

    Serial.println(F("? Initializing modem..."));
    SerialAT.begin(UART_BAUD, SERIAL_8N1, PIN_RX, PIN_TX);
    
    if (!modem.init()) {
        Serial.println(F("? Failed to initialize modem."));
        return;
    }

    Serial.println(F("? Setting network mode to LTE..."));
    modem.sendAT(F("+CNMP=38")); // 38 is for LTE only
    if (modem.waitResponse(10000L) != 1) {
        Serial.println(F("? Failed to set network mode."));
    }
    
    Serial.println(F("? Setting full functionality..."));
    modem.sendAT(F("+CFUN=1"));
    if (modem.waitResponse(10000L) != 1) {
        Serial.println(F("? Failed to set full functionality."));
    }

    Serial.println(F("? Waiting for network..."));
    if (!modem.waitForNetwork()) {
        Serial.println(F("? Failed to connect to network."));
        return;
    }
    Serial.println(F("? Network connected."));

    Serial.println(F("? Connecting to GPRS..."));
    if (!modem.gprsConnect(apn)) {
        Serial.println(F("? GPRS connection failed."));
        return;
    }

    String ip = modem.getLocalIP();
    Serial.println("? GPRS Connected. IP: " + ip);

    Serial.println(F("? Initializing SD card..."));
    SPI.begin(SD_SCLK, SD_MISO, SD_MOSI, SD_CS);
    if (!SD.begin(SD_CS)) {
        Serial.println(F("? SD Card initialization failed."));
        return;
    }
    Serial.println(F("? SD Card initialized."));
}

void loop() {
    uploadFile("/sigma2.wav");
    delay(30000); // Wait before next upload attempt
}

void uploadFile(const char* filename) {
    File file = SD.open(filename, FILE_READ);
    if (!file) {
        Serial.println("? Failed to open file for reading");
        return;
    }

    size_t fileSize = file.size();
    Serial.println("? Preparing to upload " + String(filename) + " (" + String(fileSize) + " bytes)");

    size_t offset = 0;
    while (offset < fileSize) {
        size_t chunkSize = min((size_t)CHUNK_SIZE, fileSize - offset);
        
        Serial.println("  Attempting to send chunk at offset " + String(offset) + "...");

        if (!client.connect(server, port)) {
            Serial.println("? HTTPS connection failed.");
            delay(5000);
            continue;
        }
        
        Serial.println("? Connected to server.");

        // Prepare HTTP POST request
        client.println("POST " + String(resource) + " HTTP/1.1");
        client.println("Host: " + String(server));
        client.println("Connection: close");
        client.println("Content-Type: application/octet-stream");
        client.println("X-Filename: " + String(filename).substring(1));
        client.println("X-Chunk-Offset: " + String(offset));
        client.println("X-Chunk-Size: " + String(chunkSize));
        client.println("X-Total-Size: " + String(fileSize));
        client.println("Content-Length: " + String(chunkSize));
        client.println();

        // Write file chunk to request body
        uint8_t buffer[1024];
        size_t written = 0;
        file.seek(offset);
        while (written < chunkSize) {
            size_t toRead = min((size_t)sizeof(buffer), chunkSize - written);
            size_t read = file.read(buffer, toRead);
            if (read == 0) break;
            client.write(buffer, read);
            written += read;
        }

        Serial.println("? Chunk sent. Waiting for response...");

        // Wait for response
        unsigned long timeout = millis();
        while (client.connected() && !client.available() && millis() - timeout < 30000L) {
            delay(100);
        }

        if (client.available()) {
            String line = client.readStringUntil('\n');
            Serial.println("? Server response: " + line);
            if (line.indexOf("200 OK") != -1) {
                 Serial.println("? Chunk uploaded successfully.");
                 offset += chunkSize;
            } else {
                 Serial.println("? Server returned an error. Retrying chunk.");
                 delay(5000); // Wait before retrying
            }
        } else {
            Serial.println("? No response from server or timeout. Retrying chunk.");
            delay(5000);
        }
        
        client.stop();
        Serial.println("? Connection closed.");
    }

    file.close();
    Serial.println("? File upload complete.");
}
