
#define TINY_GSM_MODEM_SIM7600
#define TINY_GSM_DEBUG Serial
#define SerialAT Serial1

#include <TinyGsmClient.h>
#include <SD.h>
#include "FS.h"

// Modem Serial port
const int RX_PIN = 16;
const int TX_PIN = 17;

// SD Card pin
const int SD_CS = 5;

// Server details
const char server[] = "6000-firebase-studio-1753223410587.cluster-73qgvk7hjjadkrjeyexca5ivva.cloudworkstations.dev";
const char resource[] = "/api/upload";
const int port = 443;
const char apn[] = "internet";
const char gprsUser[] = "";
const char gprsPass[] = "";

TinyGsm modem(SerialAT);
TinyGsmClient client(modem);

void setup() {
    Serial.begin(115200);
    delay(10);

    Serial.println(F("Initializing modem..."));
    SerialAT.begin(115200, SERIAL_8N1, RX_PIN, TX_PIN);
    if (!modem.init()) {
        Serial.println(F("Failed to initialize modem!"));
        while (1);
    }
    
    // Set to LTE only mode
    modem.sendAT(GF("+CNMP=38"));
    if (modem.waitResponse(1000L) != 1) {
      Serial.println(F("Failed to set network mode to LTE"));
    }
    
    // Set to full functionality
    modem.sendAT(GF("+CFUN=1"));
    if (modem.waitResponse(1000L) != 1) {
      Serial.println(F("Failed to set full functionality"));
    }

    Serial.println(F("Waiting for network..."));
    if (!modem.waitForNetwork()) {
        Serial.println(F("Failed to connect to network!"));
        while (1);
    }
    Serial.println(F("Network connected."));

    Serial.println(F("Connecting to GPRS..."));
    if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
        Serial.println(F("Failed to connect to GPRS!"));
        while (1);
    }
    Serial.println(F("GPRS connected."));
    Serial.print(F("IP Address: "));
    Serial.println(modem.getLocalIP());

    Serial.println(F("Initializing SD card..."));
    if (!SD.begin(SD_CS)) {
        Serial.println(F("SD card initialization failed!"));
        while (1);
    }
    Serial.println(F("SD card initialized."));

    // Select the file to upload
    const char* filename = "/sigma2.wav"; // Change this to the file you want to upload
    uploadFile(filename);
}

void loop() {
    // Everything is done in setup
}

void uploadFile(const char* filename) {
    File file = SD.open(filename, FILE_READ);
    if (!file) {
        Serial.println(F("Failed to open file for reading"));
        return;
    }

    size_t fileSize = file.size();
    Serial.print(F("Uploading file: "));
    Serial.print(filename);
    Serial.print(F(" ("));
    Serial.print(fileSize);
    Serial.println(F(" bytes)"));

    Serial.print(F("Connecting to server: "));
    Serial.println(server);
    
    if (!client.connect(server, port)) {
         Serial.println(F("... failed to connect to server"));
         file.close();
         return;
    }

    if (!client.sslConnect(server, port)) {
        Serial.println(F("... SSL connection failed"));
        file.close();
        return;
    }
    Serial.println(F("... connected"));

    // Make a POST request
    client.print(F("POST "));
    client.print(resource);
    client.println(F(" HTTP/1.1"));

    // Send headers
    client.print(F("Host: "));
    client.println(server);
    client.println(F("Connection: close"));
    client.println(F("Transfer-Encoding: chunked"));
    client.print(F("X-Filename: "));
    client.println(filename);

    // End of headers
    client.println();

    // Send file content in chunks
    const size_t bufferSize = 1024;
    uint8_t buffer[bufferSize];
    while (file.available()) {
        size_t bytesRead = file.read(buffer, bufferSize);
        if (bytesRead > 0) {
            client.print(bytesRead, HEX); // Chunk size in hex
            client.print(F("\r\n"));
            client.write(buffer, bytesRead);
            client.print(F("\r\n"));
            Serial.print(".");
        }
    }
    
    // End of chunked data
    client.println(F("0\r\n"));
    Serial.println();
    Serial.println(F("File upload complete."));

    // Read server response
    Serial.println(F("Server response:"));
    while (client.connected()) {
        while (client.available()) {
            char c = client.read();
            Serial.write(c);
        }
    }

    client.stop();
    file.close();
    Serial.println(F("Connection closed."));
}
