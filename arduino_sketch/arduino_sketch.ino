
#define TINY_GSM_MODEM_SIM7600
#define TINY_GSM_DEBUG Serial

// Set serial for debug console (to Serial Monitor, default speed 115200)
#define SerialMon Serial
// Set serial for AT commands (to SIM7600 module)
#define SerialAT Serial1

#include <TinyGsmClient.h>
#include <SPI.h>
#include <SD.h>
#include <FS.h>

// Define the serial pins for communication with the SIM7600 module
#define UART_TX 27
#define UART_RX 26

// SIM card and network credentials
const char apn[] = "internet"; // APN (Access Point Name)
const char gprsUser[] = "";    // GPRS User
const char gprsPass[] = "";    // GPRS Password

// Server details
const char server[] = "6000-firebase-studio-1753223410587.cluster-73qgvk7hjjadkrjeyexca5ivva.cloudworkstations.dev";
const int port = 443;

// File to upload
const char* filename = "/sigma2.wav";

// Size of each chunk to upload (4KB)
#define CHUNK_SIZE 4096

// TinyGSM client for secure connection
TinyGsm modem(SerialAT);
TinyGsmClientSecure client(modem);

void setup() {
    // Start serial communication for debugging
    SerialMon.begin(115200);
    delay(10);

    // Set up serial communication with the SIM7600 module
    SerialAT.begin(115200, SERIAL_8N1, UART_RX, UART_TX);
    delay(3000);

    SerialMon.println("? Booting...");

    // Initialize the SD card
    if (!SD.begin()) {
        SerialMon.println("! SD card initialization failed!");
        while (1);
    }
    SerialMon.println("? SD card ready.");

    // Initialize the modem
    SerialMon.println("? Initializing modem...");
    if (!modem.init()) {
        SerialMon.println("! Failed to initialize modem!");
        return;
    }

    // Set modem to full functionality
    modem.sendAT("+CFUN=1");
    if (modem.waitResponse(10000L) != 1) {
        SerialMon.println("! Failed to set modem to full functionality.");
    }

    // Set network mode to LTE only for 4G
    modem.sendAT("+CNMP=38");
     if (modem.waitResponse(10000L) != 1) {
        SerialMon.println("! Failed to set network mode to LTE.");
    }

    // Wait for network registration
    SerialMon.println("? Waiting for network registration...");
    if (!modem.waitForNetwork()) {
        SerialMon.println("! Network registration failed!");
        return;
    }
    SerialMon.println("? Registered on network.");

    // Connect to GPRS
    SerialMon.println("? Connecting to GPRS...");
    if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
        SerialMon.println("! GPRS connection failed!");
        return;
    }

    // Get IP address
    String ip = modem.getLocalIP();
    SerialMon.println("? GPRS Connected. IP: " + ip);
    
    // Upload the file
    uploadFile(filename);
}

void loop() {
    // The main logic is in setup() for this example.
    // The device will upload the file and then do nothing.
    delay(10000);
}

void uploadFile(const char* path) {
    File file = SD.open(path, FILE_READ);
    if (!file) {
        SerialMon.println("! Failed to open file for reading: " + String(path));
        return;
    }

    size_t fileSize = file.size();
    SerialMon.println("? Preparing to upload " + String(path) + " (" + String(fileSize) + " bytes)");

    size_t offset = 0;
    while (offset < fileSize) {
        size_t chunkSize = CHUNK_SIZE;
        if (offset + chunkSize > fileSize) {
            chunkSize = fileSize - offset;
        }

        SerialMon.println("? Uploading chunk at offset " + String(offset) + " (" + String(chunkSize) + " bytes)...");

        if (!client.connect(server, port)) {
            SerialMon.println("! Connection to server failed!");
            file.close();
            return;
        }

        // Send HTTP headers
        String headers = "POST /api/upload HTTP/1.1\r\n";
        headers += "Host: " + String(server) + "\r\n";
        headers += "Content-Type: application/octet-stream\r\n";
        headers += "X-Filename: " + String(path).substring(1) + "\r\n";
        headers += "X-Chunk-Offset: " + String(offset) + "\r\n";
        headers += "X-Chunk-Size: " + String(chunkSize) + "\r\n";
        headers += "X-Total-Size: " + String(fileSize) + "\r\n";
        headers += "Content-Length: " + String(chunkSize) + "\r\n";
        headers += "Connection: close\r\n";
        headers += "\r\n";
        client.print(headers);

        // Send the file chunk
        uint8_t buffer[256];
        size_t sentBytes = 0;
        while (sentBytes < chunkSize) {
            size_t toRead = chunkSize - sentBytes;
            if (toRead > sizeof(buffer)) {
                toRead = sizeof(buffer);
            }
            int readBytes = file.read(buffer, toRead);
            if (readBytes <= 0) break;
            client.write(buffer, readBytes);
            sentBytes += readBytes;
        }

        // Wait for the server's response
        long startTime = millis();
        while (client.connected() && millis() - startTime < 30000L) {
            if (client.available()) {
                String line = client.readStringUntil('\n');
                SerialMon.println("[SVR RECV] " + line);
                if (line.startsWith("HTTP/1.1 200")) {
                     SerialMon.println("? Chunk uploaded successfully.");
                }
            }
        }

        client.stop();
        SerialMon.println("? Connection closed.");

        offset += chunkSize;
    }

    file.close();
    SerialMon.println("? File upload finished.");
}
