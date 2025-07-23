// Define the serial connections for the modem
#define SerialMon Serial
#define SerialAT Serial1

// Modem and SD card pin definitions
#define UART_BAUD 115200
#define PIN_TX 27
#define PIN_RX 26
#define SD_CS 5

#define TINY_GSM_MODEM_SIM7600
#define TINY_GSM_USE_GPRS true
#define TINY_GSM_USE_WIFI false

#include <TinyGsmClient.h>
#include <SPI.h>
#include <SD.h>

#ifdef DUMP_AT_COMMANDS
#include <StreamDebugger.h>
StreamDebugger debugger(SerialAT, SerialMon);
TinyGsm modem(debugger);
#else
TinyGsm modem(SerialAT);
#endif

// Server details
const char server[] = "cellular-data-streamer-5xx1xBCs-dev.web.app";
const char resource[] = "/api/upload";
const int port = 443;

// GPRS credentials
const char apn[] = "VZWINTERNET";  // APN for Verizon, change if needed
const char gprsUser[] = "";
const char gprsPass[] = "";

TinyGsmClient client(modem);
const int CHUNK_SIZE = 1024; // 1KB chunk size

// Function to send AT command and wait for a specific response
bool sendATCommand(const char* cmd, const char* expect, unsigned long timeout = 10000) {
    SerialMon.print("Sending: ");
    SerialMon.println(cmd);
    SerialAT.println(cmd);

    unsigned long start = millis();
    String res = "";
    while (millis() - start < timeout) {
        while (SerialAT.available()) {
            res += (char)SerialAT.read();
        }
        if (res.indexOf(expect) != -1) {
            SerialMon.print("Received: ");
            SerialMon.println(res);
            return true;
        }
    }
    SerialMon.print("Timeout or unexpected response: ");
    SerialMon.println(res);
    return false;
}

void setupModem() {
    SerialMon.println("Initializing modem...");
    modem.restart();
    
    // Set modem to full functionality
    if (!modem.setPhoneFunctionality(1)) {
        SerialMon.println("Failed to set phone functionality.");
        return;
    }
    delay(1000);

    // Set to 4G/LTE mode
    if (!sendATCommand("AT+CNMP=38", "OK")) {
        SerialMon.println("Failed to set LTE mode.");
        // continue anyway
    }
    delay(1000);
}

void setup() {
    SerialMon.begin(115200);
    delay(10);

    SerialAT.begin(UART_BAUD, SERIAL_8N1, PIN_RX, PIN_TX);
    delay(6000);

    setupModem();

    SerialMon.println("Waiting for network...");
    if (!modem.waitForNetwork()) {
        SerialMon.println(" failed");
        while (true);
    }
    SerialMon.println(" success");

    SerialMon.print("Connecting to ");
    SerialMon.print(apn);
    if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
        SerialMon.println(" failed");
        while (true);
    }
    SerialMon.println(" success");

    if (SD.begin(SD_CS)) {
        SerialMon.println("SD card initialized.");
        uploadFile("/test.wav"); // Change to your filename
    } else {
        SerialMon.println("SD card failed to initialize");
    }
}

void uploadFile(const char* filename) {
    File file = SD.open(filename);
    if (!file) {
        SerialMon.println("Failed to open file for reading");
        return;
    }

    size_t fileSize = file.size();
    SerialMon.print("File size: ");
    SerialMon.println(fileSize);
    
    if (!modem.https_begin()) {
        SerialMon.println("Failed to start HTTPS");
        file.close();
        return;
    }

    modem.https_set_server(server, port);

    byte buffer[CHUNK_SIZE];
    size_t bytesRead = 0;
    size_t totalBytesSent = 0;
    
    while (file.available()) {
        bytesRead = file.read(buffer, sizeof(buffer));

        SerialMon.print("Sending chunk of size: ");
        SerialMon.println(bytesRead);

        if (!modem.https_begin_chunk(HTTP_POST, resource)) {
            SerialMon.println("Failed to begin chunked request");
            break;
        }

        modem.https_header("Content-Type", "application/octet-stream");
        modem.https_header("X-Filename", filename);
        modem.https_header("X-Chunk-Offset", String(totalBytesSent).c_str());
        modem.https_header("X-Chunk-Size", String(bytesRead).c_str());
        modem.https_header("X-Total-Size", String(fileSize).c_str());

        if(!modem.https_chunk(buffer, bytesRead)){
            SerialMon.println("Failed to send chunk data");
            break;
        }

        if(!modem.https_end_chunk()){
             SerialMon.println("Failed to end chunk");
             break;
        }
        
        int httpCode = modem.https_code();
        SerialMon.print("HTTP status code: ");
        SerialMon.println(httpCode);
        if (httpCode != 200) {
            SerialMon.println("Server returned error.");
            break;
        }
        
        String response = modem.https_read_string();
        SerialMon.print("Server response: ");
        SerialMon.println(response);

        totalBytesSent += bytesRead;
    }

    modem.https_end();
    file.close();

    if (totalBytesSent == fileSize) {
        SerialMon.println("File upload complete.");
    } else {
        SerialMon.println("File upload failed.");
    }
}

void loop() {
    // Done
}
