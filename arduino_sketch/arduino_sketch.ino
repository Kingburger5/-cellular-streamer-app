
// This sketch is designed for an ESP32 with a SIM7600 module to upload a file from an SD card to a web server.
// It uses the TinyGSM library to manage the modem and a secure client for HTTPS communication.

// Define the serial debug port
#define SerialMon Serial

// Define the hardware serial port for the modem
#define SerialAT Serial1

// Configure TinyGSM library
#define TINY_GSM_MODEM_SIM7600
#define TINY_GSM_USE_GPRS true
#define TINY_GSM_USE_WIFI false

// Include necessary libraries
#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <TinyGsmClient.h>

// Server details
const char server[] = "6000-firebase-studio-1753223410587.cluster-73qgvk7hjjadkrjeyexca5ivva.cloudworkstations.dev";
const char resource[] = "/api/upload";
const int port = 443;
const char apn[] = "internet"; // Your GPRS APN
const char gprsUser[] = "";    // GPRS User, if required
const char gprsPass[] = "";    // GPRS Password, if required

// Pin definitions
#define UART_BAUD   115200
#define PIN_TX      17 // ESP32 TX to SIM7600 RX
#define PIN_RX      16 // ESP32 RX to SIM7600 TX
#define SD_CS       5  // The Chip Select pin for the SD card

// File to upload
const char* filename = "/sigma2.wav";

// Chunk size for upload
const size_t CHUNK_SIZE = 4096;

// Modem and client objects
TinyGsm modem(SerialAT);
TinyGsmClientSecure client(modem);

void setup() {
    // Start serial communication for debugging
    SerialMon.begin(115200);
    delay(10);
    SerialMon.println(F("? Booting..."));

    // Initialize SD card
    if (!SD.begin(SD_CS)) {
        SerialMon.println(F("? SD card initialization failed!"));
        while (1);
    }
    SerialMon.println(F("? SD card ready."));

    // Set up modem serial port
    SerialAT.begin(UART_BAUD, SERIAL_8N1, PIN_RX, PIN_TX);
    delay(6000);

    // Initialize modem
    SerialMon.println(F("? Initializing modem..."));
    if (!modem.init()) {
        SerialMon.println(F("? Failed to initialize modem. Halting."));
        while (1);
    }
     SerialMon.println(F("? Modem initialized."));
}

void loop() {
    // Check network and GPRS connection
    if (!modem.isNetworkRegistered()) {
        SerialMon.println(F("? Network not registered. Waiting..."));
        delay(5000);
        return;
    }

    if (!modem.isGprsConnected()) {
        SerialMon.println(F("? GPRS not connected. Connecting..."));
        if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
            SerialMon.println(F("? GPRS connection failed."));
            delay(5000);
            return;
        }
        SerialMon.println(F("? GPRS connected."));
    }

    // Open file from SD card
    File file = SD.open(filename, FILE_READ);
    if (!file) {
        SerialMon.println(F("? Failed to open file for reading. Halting."));
        while (1);
    }

    size_t fileSize = file.size();
    SerialMon.print(F("? Preparing to upload "));
    SerialMon.print(filename);
    SerialMon.print(F(" ("));
    SerialMon.print(fileSize);
    SerialMon.println(F(" bytes)"));

    // Upload file in chunks
    size_t bytesUploaded = 0;
    while (bytesUploaded < fileSize) {
        size_t bytesToSend = min(CHUNK_SIZE, fileSize - bytesUploaded);

        // Establish secure connection
        SerialMon.print(F("? Connecting to server: "));
        SerialMon.println(server);
        if (!client.connect(server, port)) {
            SerialMon.println(F("? Connection to server failed. Retrying..."));
            delay(5000);
            continue; // Retry connection
        }
        SerialMon.println(F("? Connection successful."));

        // Send HTTP headers
        client.print(String("POST ") + resource + " HTTP/1.1\r\n");
        client.print(String("Host: ") + server + "\r\n");
        client.print("Connection: close\r\n");
        client.print(String("X-Filename: ") + filename + "\r\n");
        client.print(String("X-Total-Size: ") + fileSize + "\r\n");
        client.print(String("X-Chunk-Offset: ") + bytesUploaded + "\r\n");
        client.print(String("X-Chunk-Size: ") + bytesToSend + "\r\n");
        client.print("Content-Type: application/octet-stream\r\n");
        client.print(String("Content-Length: ") + bytesToSend + "\r\n");
        client.print("\r\n");

        // Send file chunk
        uint8_t buffer[CHUNK_SIZE];
        size_t bytesRead = file.read(buffer, bytesToSend);
        
        if (bytesRead != bytesToSend) {
            SerialMon.println(F("? Error reading from SD card."));
            client.stop();
            file.close();
            return;
        }

        SerialMon.print(F("? Uploading chunk at offset "));
        SerialMon.print(bytesUploaded);
        SerialMon.print(F(" ("));
        SerialMon.print(bytesToSend);
        SerialMon.println(F(" bytes)..."));

        client.write(buffer, bytesToSend);

        // Wait for server response
        unsigned long timeout = millis();
        while (client.connected() && millis() - timeout < 30000) {
            if (client.available()) {
                String line = client.readStringUntil('\n');
                SerialMon.print("[SERVER] ");
                SerialMon.println(line);
                if (line.startsWith("HTTP/1.1 200 OK")) {
                   // Success, can add more logic here if needed
                }
            }
        }
        
        // Disconnect and update progress
        client.stop();
        SerialMon.println(F("? Client disconnected."));
        bytesUploaded += bytesToSend;
        
        // A small delay between chunks can help with network stability
        delay(1000); 
    }

    SerialMon.println(F("? File upload complete."));
    file.close();

    // Wait forever after one successful upload
    while (true) {
        delay(1000);
    }
}
