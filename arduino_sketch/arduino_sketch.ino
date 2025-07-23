#define TINY_GSM_MODEM_SIM7600
#define TINY_GSM_RX_BUFFER 1024

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <TinyGsmClient.h>

// Pin definitions for UART communication with the modem
#define UART_BAUD   115200
#define PIN_TX      17
#define PIN_RX      16

// Pin definitions for the SD Card
#define SD_MISO     13
#define SD_MOSI     12
#define SD_SCLK     14
#define SD_CS       5

// Server details
const char server[] = "6000-firebase-studio-1753223410587.cluster-73qgvk7hjjadkrjeyexca5ivva.cloudworkstations.dev";
const char resource[] = "/api/upload";
const int  port = 443;

// APN details for your network provider
const char apn[] = "internet";
const char gprsUser[] = "";
const char gprsPass[] = "";

#define SerialMon Serial
#define SerialAT Serial1

// TinyGSM modem and client
TinyGsm modem(SerialAT);
TinyGsmClientSecure client(modem, 0);


// File system
File root;

// Constants
const size_t CHUNK_SIZE = 4096; // 4KB chunks

void setup() {
    SerialMon.begin(115200);
    delay(10);

    SerialMon.println(F("? Booting..."));

    // Initialize SD card
    SPI.begin(SD_SCLK, SD_MISO, SD_MOSI, SD_CS);
    if (!SD.begin(SD_CS)) {
        SerialMon.println(F("! SD Card initialization failed!"));
        while (1);
    }
    SerialMon.println(F("? SD card ready."));

    // Initialize Modem
    SerialAT.begin(UART_BAUD, SERIAL_8N1, PIN_RX, PIN_TX);
    
    SerialMon.println(F("? Initializing modem..."));
    if (!modem.init()) {
        SerialMon.println(F("! Failed to initialize modem."));
        return;
    }
    
    // Set to 4G/LTE mode
    modem.sendAT(F("+CNMP=38")); // 38 for LTE only
    modem.waitResponse();
    
    modem.sendAT(F("+CFUN=1")); // Full functionality
    modem.waitResponse();
    
    String modemInfo = modem.getModemInfo();
    SerialMon.print(F("? Modem Info: "));
    SerialMon.println(modemInfo);

    // Connect to GPRS
    SerialMon.print(F("? Connecting to APN: "));
    SerialMon.println(apn);
    if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
        SerialMon.println(F("! GPRS connection failed."));
        return;
    }

    SerialMon.println(F("? GPRS connected."));
    
    root = SD.open("/");
    uploadFile(root);
}

void loop() {
    // Everything is done in setup for this sketch
}

void uploadFile(File dir) {
    while (true) {
        File entry = dir.openNextFile();
        if (!entry) {
            // no more files
            break;
        }

        if (entry.isDirectory()) {
            // Skip directories
            entry.close();
            continue;
        }

        const char* filename = entry.name();
        size_t totalSize = entry.size();
        SerialMon.printf("? Preparing to upload %s (%d bytes)\n", filename, totalSize);
        
        size_t offset = 0;
        while (offset < totalSize) {
            size_t chunkSize = totalSize - offset > CHUNK_SIZE ? CHUNK_SIZE : totalSize - offset;

            SerialMon.printf("  > Uploading chunk at offset %d (%d bytes)...\n", offset, chunkSize);
            
            // Establish secure connection for each chunk
            if (!client.connect(server, port)) {
                SerialMon.println("! Connection failed.");
                entry.close();
                return; // Or handle retry
            }
            SerialMon.println("  > Connected to server.");

            // Construct and send HTTP headers
            client.printf("POST %s HTTP/1.1\r\n", resource);
            client.printf("Host: %s\r\n", server);
            client.println("Connection: close");
            client.printf("Content-Length: %d\r\n", chunkSize);
            client.printf("X-Filename: %s\r\n", filename);
            client.printf("X-Total-Size: %d\r\n", totalSize);
            client.printf("X-Chunk-Offset: %d\r\n", offset);
            client.printf("X-Chunk-Size: %d\r\n", chunkSize);
            client.println(); // End of headers

            // Send the file chunk
            size_t bytesSent = 0;
            char buffer[256];
            entry.seek(offset); // Go to the correct position in the file
            size_t toSend = chunkSize;
            while(toSend > 0) {
                size_t bytesRead = entry.read((uint8_t*)buffer, min(toSend, sizeof(buffer)));
                if(bytesRead == 0) break; // End of file chunk
                client.write((const uint8_t*)buffer, bytesRead);
                bytesSent += bytesRead;
                toSend -= bytesRead;
            }

            SerialMon.printf("  > Sent %d bytes.\n", bytesSent);
            
            // Wait for server response
            unsigned long timeout = millis();
            while (client.connected() && !client.available() && millis() - timeout < 30000L) {
                delay(100);
            }

            if (client.available()) {
                String line = client.readStringUntil('\n');
                SerialMon.printf("  < Server Response: %s\n", line.c_str());
                // You can add more logic here to parse the response if needed
            } else {
                SerialMon.println("  < No response from server.");
            }
            
            client.stop();
            SerialMon.println("  > Connection closed.");

            offset += chunkSize;
        }

        SerialMon.printf("? Finished uploading %s\n", filename);
        entry.close();
    }
}
