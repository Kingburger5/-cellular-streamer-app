
#define TINY_GSM_MODEM_SIM7600
#include <Arduino.h>
#include <SD.h>
#include <SPI.h>

#define MODEM_RX 16
#define MODEM_TX 17
#define MODEM_BAUD 115200

#define SD_CS 5

#include <TinyGsmClient.h>
#include <StreamDebugger.h>

HardwareSerial modemSerial(1);
TinyGsm modem(modemSerial);
StreamDebugger debugger(modemSerial, Serial);

const char apn[] = "hologram";
const char user[] = "";
const char pass[] = "";

const char *server = "cellular-data-streamer.web.app";
const int port = 443;
const char *resource = "/api/upload";

const int MAX_RETRIES = 3;
const int CHUNK_SIZE = 4096; // 4KB chunk size
const int COMMAND_TIMEOUT = 10000;
const int RESPONSE_TIMEOUT = 10000;

void sendATCommand(const char* cmd, const char* expected_response = "OK", unsigned long timeout = 1000) {
    Serial.println("[AT SEND] " + String(cmd));
    modemSerial.println(cmd);
    String response;
    unsigned long start = millis();
    while (millis() - start < timeout) {
        if (modemSerial.available()) {
            char c = modemSerial.read();
            response += c;
            if (response.indexOf(expected_response) != -1) {
                // The full response might be useful for debugging.
                // Serial.println("[AT RECV] " + response);
                return;
            }
        }
    }
    Serial.println("[DEBUG] Timeout waiting for: " + String(expected_response));
    Serial.println("[DEBUG] Received: " + response);
}


bool waitForModemReady() {
  Serial.print("Initializing modem...");
  for (int i = 0; i < 30; i++) {
    if (modem.testAT(1000)) {
      SimStatus simStatus = modem.getSimStatus();
      if (simStatus == SIM_READY) {
        Serial.println("OK");
        return true;
      }
       Serial.print(".");
    }
    delay(1000);
  }
  Serial.println(" FAILED");
  return false;
}

bool configureSSL() {
    Serial.println("? Configuring SSL...");
    sendATCommand("AT+CSSLCFG=\"sslversion\",1,3");
    sendATCommand("AT+CSSLCFG=\"authmode\",1,0");
    sendATCommand("AT+HTTPSSL=1"); // Enable SSL for HTTP
    // Check if SSL is really enabled
    modemSerial.println("AT+HTTPSSL?");
    String response;
    unsigned long start = millis();
     while (millis() - start < 1000) {
        if (modemSerial.available()) {
            char c = modemSerial.read();
            response += c;
            if (response.indexOf("+HTTPSSL: 1") != -1) {
                 Serial.println("? SSL Enabled successfully.");
                return true;
            }
        }
    }
    Serial.println("? Failed to enable SSL.");
    return false;
}


bool sendChunk(File &file, const char* filename, size_t totalSize, size_t offset) {
    uint8_t chunkBuffer[CHUNK_SIZE];
    size_t bytesRead = file.read(chunkBuffer, CHUNK_SIZE);
    if (bytesRead == 0) {
        return false; // No more data to read
    }

    Serial.printf("  Attempt to send chunk at offset %u, size %u...\n", offset, bytesRead);

    // 1. Start HTTPS session
    sendATCommand("AT+CHTTPSSTART");

    // 2. Open session with server
    String opseCmd = "AT+CHTTPSOPSE=\"" + String(server) + "\"," + String(port);
    sendATCommand(opseCmd.c_str(), "+CHTTPSOPSE: 0", 15000);

    // 3. Prepare and send POST request
    String headers = "x-filename: " + String(filename) + "\r\n" +
                     "x-chunk-offset: " + String(offset) + "\r\n" +
                     "x-chunk-size: " + String(bytesRead) + "\r\n" +
                     "x-total-size: " + String(totalSize) + "\r\n";
    
    String postCmd = "AT+CHTTPSPOST=\"" + String(resource) + "\"," + String(headers.length()) + "," + String(bytesRead) + ",10000";
    sendATCommand(postCmd.c_str(), ">", COMMAND_TIMEOUT);

    // 4. Send headers and chunk data
    modemSerial.print(headers);
    modemSerial.write(chunkBuffer, bytesRead);

    // 5. Wait for server response (e.g., 200 OK or 201 Created)
    String response;
    bool success = false;
    unsigned long start = millis();
    while(millis() - start < RESPONSE_TIMEOUT) {
        if (modemSerial.available()) {
            char c = modemSerial.read();
            response += c;
            if (response.indexOf("+CHTTPS: POST,20") != -1) { // Look for 200, 201, etc.
                success = true;
                break;
            }
             if (response.indexOf("ERROR") != -1) {
                break;
            }
        }
    }

    if (success) {
        Serial.println("    ...chunk sent successfully.");
    } else {
        Serial.println("    ...chunk failed to send.");
        Serial.println("[DEBUG] Full response: " + response);
    }
    
    // 6. Close HTTPS session
    sendATCommand("AT+CHTTPSCLSE");
    sendATCommand("AT+CHTTPSSTOP");

    return success;
}

void uploadFile(const char* filename) {
    if (!SD.exists(filename)) {
        Serial.println("? File does not exist: " + String(filename));
        return;
    }

    File file = SD.open(filename, FILE_READ);
    if (!file) {
        Serial.println("? Failed to open file for reading: " + String(filename));
        return;
    }

    size_t fileSize = file.size();
    Serial.printf("? Preparing to upload %s (%u bytes)\n", filename, fileSize);

    size_t offset = 0;
    while (offset < fileSize) {
        bool success = false;
        for (int retry = 0; retry < MAX_RETRIES; retry++) {
            file.seek(offset); // Reposition file pointer for retry
            if (sendChunk(file, filename, fileSize, offset)) {
                success = true;
                break;
            }
            Serial.printf("  Retry %d/%d for chunk at offset %u\n", retry + 1, MAX_RETRIES, offset);
            delay(2000); // Wait before retrying
        }

        if (success) {
            offset += CHUNK_SIZE;
        } else {
            Serial.printf("? Failed to upload chunk at offset %u after %d retries. Aborting.\n", offset, MAX_RETRIES);
            file.close();
            return;
        }
    }

    Serial.println("? File upload completed successfully.");
    file.close();
}

void setup() {
    Serial.begin(115200);
    while (!Serial);

    Serial.println("? Booting...");

    if (!SD.begin(SD_CS)) {
        Serial.println("? SD card initialization failed!");
        while (1);
    }
    Serial.println("? SD card ready.");

    modemSerial.begin(MODEM_BAUD, SERIAL_8N1, MODEM_RX, MODEM_TX);
    delay(1000);

    if (!waitForModemReady()) {
      Serial.println("? Modem not responding. Halting.");
      while(1);
    }

    Serial.println("? Modem is ready.");

    sendATCommand("AT+CPIN?");
    sendATCommand("AT+CSQ");
    sendATCommand("AT+CREG?");
    sendATCommand("AT+CGATT?");
    
    Serial.println("? Connecting to network...");
    if (!modem.gprsConnect(apn, user, pass)) {
        Serial.println("? GPRS connection failed.");
        return;
    }
    Serial.println("? GPRS connected.");

    if(!configureSSL()){
        Serial.println("? SSL configuration failed. Halting.");
        while(1);
    }

    uploadFile("/sigma2.wav");
}

void loop() {
  // All logic is in setup for this one-shot uploader
}

    