// This is the final, corrected code for uploading a file from an SD card
// via a SIM7600G module to a server using HTTPS POST.

// Define the serial port for communication with the SIM7600 module.
#define SerialAT Serial1

// Define the pin for the SD card chip select.
#define SD_CS 5

// --- Library Includes ---
#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include "FS.h"
#define TINY_GSM_MODEM_SIM7600
#include <TinyGsmClient.h>
#include <SSLClient.h>

// --- Server Configuration ---
const char server[] = "cellular-data-streamer.web.app";
const char resource[] = "/api/upload";
const int port = 443; // HTTPS port

// --- Modem and Client Initialization ---
TinyGsm modem(SerialAT);
TinyGsmClient base_client(modem);
SSLClient client(base_client);

// --- Function to send an AT command and wait for a specific response ---
bool sendATCommand(const char* cmd, const char* expected_response, unsigned long timeout = 10000) {
    Serial.print("Sending AT: ");
    Serial.println(cmd);
    
    SerialAT.println(cmd);
    
    unsigned long start = millis();
    String response = "";
    while (millis() - start < timeout) {
        if (SerialAT.available()) {
            char c = SerialAT.read();
            response += c;
            if (response.indexOf(expected_response) != -1) {
                Serial.print("Received: ");
                Serial.println(response);
                return true;
            }
        }
    }
    Serial.print("Timeout. Full response: ");
    Serial.println(response);
    return false;
}

void setup() {
    Serial.begin(115200);
    delay(10);

    Serial.println("--- Cellular Data Streamer ---");

    // --- Initialize Modem ---
    SerialAT.begin(115200, SERIAL_8N1, 27, 26); // RX, TX pins for ESP32
    delay(6000);

    Serial.println("Initializing modem...");
    if (!modem.restart()) {
        Serial.println("Failed to restart modem. Halting.");
        while (true);
    }
    Serial.println("Modem initialized.");

    // --- Configure Modem ---
    sendATCommand("AT+CFUN=1", "OK");       // Set to full functionality
    sendATCommand("AT+CNMP=38", "OK");      // Set to LTE-only mode
    sendATCommand("AT+CGDCONT=1,\"IP\",\"\"", "OK"); // Use default APN

    // --- Connect to Network ---
    Serial.println("Waiting for network...");
    if (!modem.waitForNetwork()) {
        Serial.println("Failed to connect to network. Halting.");
        while (true);
    }
    Serial.println("Network connected.");

    Serial.println("Connecting to GPRS...");
    if (!modem.gprsConnect()) {
        Serial.println("Failed to connect to GPRS. Halting.");
        while (true);
    }
    Serial.println("GPRS connected.");

    // --- Initialize SD Card ---
    Serial.println("Initializing SD card...");
    if (!SD.begin(SD_CS)) {
        Serial.println("SD card initialization failed. Halting.");
        while (true);
    }
    Serial.println("SD card initialized.");

    // This is required for the SSLClient library.
    // It tells the library not to try and verify the server's SSL certificate.
    client.setInsecure();
}

// --- Main File Upload Function ---
void uploadFile(const char* filename) {
    File file = SD.open(filename, FILE_READ);
    if (!file) {
        Serial.println("Failed to open file for reading.");
        return;
    }

    size_t fileSize = file.size();
    Serial.print("Attempting to upload: ");
    Serial.print(filename);
    Serial.print(" (");
    Serial.print(fileSize);
    Serial.println(" bytes)");

    Serial.print("Connecting to server: ");
    Serial.println(server);

    if (!client.connect(server, port)) {
        Serial.println("Connection to server failed!");
        file.close();
        return;
    }
    Serial.println("Connected to server.");

    // --- Construct and Send HTTP Headers ---
    String headers = "POST " + String(resource) + " HTTP/1.1\r\n";
    headers += "Host: " + String(server) + "\r\n";
    headers += "X-Filename: " + String(filename) + "\r\n";
    headers += "Content-Type: application/octet-stream\r\n";
    headers += "Content-Length: " + String(fileSize) + "\r\n";
    headers += "Connection: close\r\n\r\n";
    
    client.print(headers);
    Serial.println("--- HTTP Headers Sent ---");
    Serial.println(headers);
    Serial.println("-------------------------");


    // --- Send File Content ---
    Serial.println("Sending file content...");
    const size_t bufferSize = 1024;
    byte buffer[bufferSize];
    size_t bytesSent = 0;
    
    while (file.available()) {
        size_t bytesRead = file.read(buffer, bufferSize);
        if (bytesRead > 0) {
            client.write(buffer, bytesRead);
            bytesSent += bytesRead;
            Serial.print("\rSent ");
            Serial.print(bytesSent);
            Serial.print(" of ");
            Serial.print(fileSize);
            Serial.print(" bytes");
        }
    }
    Serial.println("\nFile content sent.");

    file.close();

    // --- Wait for Server Response ---
    Serial.println("Waiting for server response...");
    unsigned long timeout = millis();
    while (client.available() == 0) {
        if (millis() - timeout > 15000) { // 15 second timeout
            Serial.println("Client timeout!");
            client.stop();
            return;
        }
    }

    // --- Print Server Response ---
    Serial.println("--- Server Response ---");
    while (client.available()) {
        Serial.write(client.read());
    }
    Serial.println("\n-----------------------");

    client.stop();
    Serial.println("Connection closed.");
}

void loop() {
    // --- Select a file to upload ---
    // Change this to the actual filename you want to upload from your SD card.
    const char* fileToUpload = "/test.txt"; 
    
    Serial.println("=================================");
    Serial.print("Starting upload for ");
    Serial.println(fileToUpload);
    Serial.println("=================================");
    
    uploadFile(fileToUpload);
    
    Serial.println("Upload attempt finished. Waiting 30 seconds before next attempt...");
    delay(30000);
}
