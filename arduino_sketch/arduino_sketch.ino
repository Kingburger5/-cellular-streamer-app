// This sketch is designed to run on an ESP32 with a SIM7600G-H module
// to upload files from an SD card to a web server via HTTPS POST.

// LIBRARIES
#define TINY_GSM_MODEM_SIM7600
#include <Arduino.h>
#include <TinyGsmClient.h>
#include <SPI.h>
#include <SD.h>
#include <SSLClient.h>
#include "utilities.h" // For root CAs

// PIN DEFINITIONS
#define UART_BAUD 115200
#define PIN_TX    27
#define PIN_RX    26
#define SD_CS     5

// MODEM & SERVER CONFIGURATION
const char server[] = "6000-firebase-studio-1753223410587.cluster-73qgvk7hjjadkrjeyexca5ivva.cloudworkstations.dev";
const char resource[] = "/api/upload";
const int  port = 443;
const char apn[] = "internet"; // Your APN
const char gprsUser[] = "";    // GPRS User, if any
const char gprsPass[] = "";    // GPRS Password, if any


// SERIAL INTERFACES
HardwareSerial SerialAT(1); // Use UART 1 for AT commands

// CLIENT OBJECTS
TinyGsm modem(SerialAT);
TinyGsmClient base_client(modem);
SSLClient client(base_client, TAs, (size_t)TAs_NUM, SD_CS); // Correctly initialize SSLClient

void setup() {
    // Start Serial Monitor
    Serial.begin(115200);
    delay(10);
    Serial.println("Starting...");

    // Initialize SD card
    if (!SD.begin(SD_CS)) {
        Serial.println("! SD Card initialization failed!");
        while (1);
    }
    Serial.println("? SD Card initialized.");

    // Initialize modem
    SerialAT.begin(UART_BAUD, SERIAL_8N1, PIN_RX, PIN_TX);
    delay(6000);

    Serial.println("? Initializing modem...");
    if (!modem.restart()) {
        Serial.println("! Failed to restart modem");
        return;
    }
    Serial.println("? Modem initialized.");

    // Set modem to 4G/LTE mode
    if (!modem.setNetworkMode(38)) { // 38 is for LTE only
        Serial.println("! Failed to set network mode to LTE");
        return;
    }
    Serial.println("? Network mode set to LTE.");

    // Connect to GPRS
    Serial.print("? Connecting to APN: ");
    Serial.println(apn);
    if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
        Serial.println("! GPRS connection failed.");
        return;
    }
    Serial.println("? GPRS connected.");

    Serial.print("? Local IP: ");
    Serial.println(modem.getLocalIP());

    // Upload the file
    uploadFile("/sigma2.wav");
}

void loop() {
    // Keep the sketch running
    delay(1000);
}

void uploadFile(const char* filePath) {
    File file = SD.open(filePath, FILE_READ);
    if (!file) {
        Serial.println("! Failed to open file for reading");
        return;
    }
    size_t fileSize = file.size();
    Serial.print("? Uploading file: ");
    Serial.print(filePath);
    Serial.print(" (");
    Serial.print(fileSize);
    Serial.println(" bytes)");

    // Establish secure connection
    Serial.print("? Connecting to server: ");
    Serial.print(server);
    if (!client.connect(server, port)) {
        Serial.println("! Connection failed.");
        file.close();
        return;
    }
    Serial.println("? Connected.");

    // Send HTTP POST headers
    client.print(String("POST ") + resource + " HTTP/1.1\r\n");
    client.print(String("Host: ") + server + "\r\n");
    client.print("Connection: close\r\n");
    client.print(String("X-Filename: ") + filePath + "\r\n");
    client.print("Content-Type: application/octet-stream\r\n");
    client.print("Transfer-Encoding: chunked\r\n");
    client.print("\r\n");
    
    // Send file in chunks
    const size_t chunkSize = 1024;
    uint8_t buffer[chunkSize];
    while (file.available()) {
        size_t bytesRead = file.read(buffer, chunkSize);
        if (bytesRead > 0) {
            // Print chunk size in hexadecimal
            client.print(String(bytesRead, HEX));
            client.print("\r\n");
            // Write chunk data
            client.write(buffer, bytesRead);
            client.print("\r\n");
            Serial.print(".");
        }
    }

    // End of chunked transfer
    client.print("0\r\n\r\n");
    Serial.println("\n? File upload complete.");

    // Read server response
    Serial.println("? Server response:");
    while (client.connected()) {
        String line = client.readStringUntil('\n');
        if (line == "\r") {
            break;
        }
        Serial.println(line);
    }
    // Read remaining body if any
    while (client.available()) {
        char c = client.read();
        Serial.print(c);
    }
    Serial.println("\n? Finished.");

    client.stop();
    file.close();
}

    