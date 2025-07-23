#define TINY_GSM_MODEM_SIM7600
#include <TinyGsmClient.h>
#include <TinyGsmClientSecure.h> // For HTTPS
#include <HardwareSerial.h>
#include <SD.h>
#include <SPI.h>

// Server details
const char server[] = "6000-firebase-studio-1753223410587.cluster-73qgvk7hjjadkrjeyexca5ivva.cloudworkstations.dev";
const char *endpoint = "/api/upload";
const int serverPort = 443; // Standard HTTPS port

// Modem settings
#define MODEM_TX 17
#define MODEM_RX 16
#define MODEM_BAUD 115200
HardwareSerial modemSerial(1);
TinyGsm modem(modemSerial);
TinyGsmClientSecure client(modem); // Use secure client for HTTPS

// SD Card settings
#define SD_CS 5

// Upload settings
#define CHUNK_SIZE 4096 // 4KB chunks
#define MAX_RETRIES 3

// Function to print detailed modem status for debugging
void printModemStatus() {
    Serial.println("--- Modem Status ---");
    Serial.print("Modem Info: ");
    Serial.println(modem.getModemInfo());

    Serial.print("Signal Quality: ");
    Serial.println(modem.getSignalQuality());

    Serial.print("SIM Status: ");
    Serial.println(modem.getSimStatus());

    Serial.print("CCID: ");
    Serial.println(modem.getSimCCID());

    Serial.print("Operator: ");
    Serial.println(modem.getOperator());
    Serial.println("--------------------");
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("?? Booting...");

    if (!SD.begin(SD_CS)) {
        Serial.println("? SD card failed to initialize.");
        while (true);
    }
    Serial.println("? SD card ready.");

    Serial.println("Initializing modem...");
    modemSerial.begin(MODEM_BAUD, SERIAL_8N1, MODEM_RX, MODEM_TX);
    delay(3000);
    if (!modem.restart()) {
        Serial.println("? Failed to restart modem.");
    }
    Serial.println("? Modem restarted.");

    printModemStatus();

    Serial.println("?? Connecting to network...");
    if (!modem.gprsConnect("vodafone", "", "")) {
        Serial.println("? GPRS connection failed.");
        while (true);
    }
    Serial.println("? GPRS connected.");
    Serial.print("Local IP: ");
    Serial.println(modem.getLocalIP());

    // This is crucial for development servers with temporary or self-signed certs
    client.setInsecure();

    // Upload your file
    uploadFile("/sigma2.wav");
}

void loop() {
    // Loop is intentionally empty
}

void uploadFile(const char *filename) {
    File file = SD.open(filename, FILE_READ);
    if (!file) {
        Serial.print("? Failed to open file: ");
        Serial.println(filename);
        return;
    }

    size_t totalSize = file.size();
    Serial.print("?? Preparing to upload ");
    Serial.print(filename);
    Serial.print(" (");
    Serial.print(totalSize);
    Serial.println(" bytes)");

    size_t offset = 0;
    bool success = true;

    while (offset < totalSize) {
        size_t chunkSize = min((size_t)CHUNK_SIZE, totalSize - offset);
        bool chunkSent = false;

        for (int retry = 0; retry < MAX_RETRIES; ++retry) {
            Serial.printf("  Attempt %d/%d to send chunk at offset %d...\n", retry + 1, MAX_RETRIES, offset);
            
            if (!client.connect(server, serverPort)) {
                Serial.printf("    ...connection to %s:%d failed.\n", server, serverPort);
                delay(2000); // Wait before retrying
                continue;
            }
            Serial.println("    ...connection successful.");

            // Construct and send headers
            String headers;
            headers += "POST " + String(endpoint) + " HTTP/1.1\r\n";
            headers += "Host: " + String(server) + "\r\n";
            headers += "X-Filename: " + String(filename).substring(1) + "\r\n"; // Remove leading '/'
            headers += "X-Total-Size: " + String(totalSize) + "\r\n";
            headers += "X-Chunk-Size: " + String(chunkSize) + "\r\n";
            headers += "X-Chunk-Offset: " + String(offset) + "\r\n";
            headers += "Content-Type: application/octet-stream\r\n";
            headers += "Content-Length: " + String(chunkSize) + "\r\n";
            headers += "Connection: close\r\n";
            headers += "\r\n";
            
            Serial.println("--- Sending Headers ---");
            Serial.print(headers);
            Serial.println("-----------------------");

            client.print(headers);

            // Read file chunk and send
            uint8_t buffer[CHUNK_SIZE];
            file.seek(offset);
            size_t bytesRead = file.read(buffer, chunkSize);
            if (bytesRead != chunkSize) {
                 Serial.println("? Error reading from SD card.");
                 client.stop();
                 break; // Break retry loop
            }
            
            client.write(buffer, chunkSize);

            // Wait for and print response
            unsigned long timeout = millis();
            bool responseOk = false;
            String response = "";
            while (client.connected() && millis() - timeout < 10000) {
                if (client.available()) {
                    response += client.readStringUntil('\n') + "\n";
                    if (response.indexOf("HTTP/1.1 200") != -1 || response.indexOf("HTTP/1.1 201") != -1) {
                        responseOk = true;
                    }
                }
            }
            
            Serial.println("--- Server Response ---");
            Serial.print(response);
            Serial.println("-----------------------");

            if (responseOk) {
                Serial.printf("    ...chunk at offset %d sent successfully.\n", offset);
                chunkSent = true;
                client.stop();
                break; // Success, exit retry loop
            } else {
                Serial.println("    ...upload of chunk failed (bad response).");
                client.stop();
                delay(2000); // Wait before retrying
            }
        }

        if (!chunkSent) {
            Serial.printf("? Failed to upload chunk at offset %d after %d retries. Aborting.\n", offset, MAX_RETRIES);
            success = false;
            break; // Abort file upload
        }

        offset += chunkSize;
    }

    if (success) {
        Serial.println("? File upload finished successfully.");
    } else {
        Serial.println("? File upload failed.");
    }

    file.close();
}
