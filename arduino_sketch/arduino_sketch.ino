// =================================================================
// Configuration
// =================================================================

#define TINY_GSM_MODEM_SIM7600
#define TINY_GSM_DEBUG Serial // Enable debug prints

// Modem pins
#define MODEM_TX 17
#define MODEM_RX 16
#define MODEM_BAUD 115200

// SD Card
#define SD_CS 5
#define CHUNK_SIZE 4096 // 4KB chunks

// Server details
const char server[] = "6000-firebase-studio-1753223410587.cluster-73qgvk7hjjadkrjeyexca5ivva.cloudworkstations.dev";
const char endpoint[] = "/api/upload";
const char apn[] = "internet"; // Use "internet" for One NZ

// =================================================================
// Includes and Global Objects
// =================================================================

#include <HardwareSerial.h>
#include <TinyGsmClient.h>
#include <SD.h>
#include <SPI.h>

HardwareSerial XCOM(1); // Use UART1 for communication with the modem
TinyGsm modem(XCOM);

// Forward declarations to solve 'not declared in this scope' errors
bool sendATCommand(const char* cmd, unsigned long timeout, const char* expect, String& response, const char* end_marker);
bool sendATCommand(const __FlashStringHelper* cmd, unsigned long timeout, const char* expect, String& response, const char* end_marker);


// =================================================================
// Main Setup and Loop
// =================================================================

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println(F("? Booting..."));

  // --- Initialize SD Card ---
  if (!SD.begin(SD_CS)) {
    Serial.println(F("? SD card failed. Halting."));
    while (true);
  }
  Serial.println(F("? SD card ready."));

  // --- Initialize Modem ---
  XCOM.begin(MODEM_BAUD, SERIAL_8N1, MODEM_RX, MODEM_TX);
  Serial.println(F("? Initializing modem..."));
  
  // Wait for modem to become responsive
  Serial.println(F("? Waiting for modem to be ready..."));
  unsigned long start = millis();
  while (millis() - start < 10000) {
    if (modem.testAT()) {
      break;
    }
    delay(500);
  }

  if (!modem.isGprsConnected()) {
    Serial.println(F("? Modem is ready."));
    printModemStatus();

    // --- Wait for Network Registration ---
    Serial.println(F("? Waiting for network registration..."));
    RegStatus reg_status = modem.getRegistrationStatus();
    start = millis();
    while (reg_status != REG_OK_HOME && reg_status != REG_OK_ROAMING && (millis() - start < 60000L)) {
      delay(1000);
      reg_status = modem.getRegistrationStatus();
      Serial.println("Network registration status: " + String((int)reg_status));
    }
    
    if (reg_status == REG_OK_HOME || reg_status == REG_OK_ROAMING) {
      Serial.println(F("? Registered on network."));
    } else {
      Serial.println(F("? Failed to register on network. Halting."));
      while (true);
    }

    // --- Connect to GPRS ---
    if (manualGprsConnect()) {
        String ip;
        sendATCommand(F("AT+CGPADDR=1"), 5000, "+CGPADDR:", ip, "OK");
        if (ip.length() > 0) {
            ip.remove(0, ip.indexOf(':') + 2);
            Serial.println("? GPRS Connected. IP: " + ip);
        }
    } else {
      Serial.println(F("? GPRS connection failed. Halting."));
      while (true);
    }
  } else {
    Serial.println(F("? Already connected."));
  }

  // --- Upload File ---
  uploadFile("/sigma2.wav");
}

void loop() {
  // Keep the device awake, but do nothing in the loop
  delay(1000);
}

// =================================================================
// Core Functions
// =================================================================

/**
 * Manually connects to GPRS using AT commands.
 */
bool manualGprsConnect() {
  Serial.println(F("? Connecting to GPRS..."));
  String res;
  if (!sendATCommand(F("AT+CGDCONT=1,\"IP\",\"internet\""), 5000, "OK", res, nullptr)) return false;
  if (!sendATCommand(F("AT+CGACT=1,1"), 5000, "OK", res, nullptr)) return false;
  return true;
}

/**
 * Opens an HTTPS session with the server.
 */
bool openHttpsSession() {
  String res;
  // Initialize the service
  if (!sendATCommand(F("AT+CHTTPSSTART"), 20000, "+CHTTPSSTART: 0", res, nullptr)) {
      Serial.println(F("? Failed to start HTTPS service."));
      return false;
  }
  
  // Set the URL parameter BEFORE opening the session
  String url_cmd = "AT+CHTTPSPARA=\"URL\",\"" + String(endpoint) + "\"";
  if (!sendATCommand(url_cmd.c_str(), 20000, "OK", res, nullptr)) {
      Serial.println(F("? Failed to set URL parameter."));
      return false;
  }

  // Open the session now that the URL is set
  String open_cmd = "AT+CHTTPSOPSE=\"" + String(server) + "\",443";
  if (!sendATCommand(open_cmd.c_str(), 30000, "+CHTTPSOPSE: 0", res, nullptr)) {
      Serial.println(F("? Failed to open HTTPS session."));
      return false;
  }

  return true;
}


/**
 * Sets the required HTTP headers for the file chunk upload.
 */
bool setRequestHeaders(const char* filename, size_t totalSize, size_t offset, size_t chunkSize) {
    String res, cmd;
    
    cmd = "AT+CHTTPSPARA=\"USERDATA\",\"Content-Type: application/octet-stream\"";
    if (!sendATCommand(cmd.c_str(), 5000, "OK", res, nullptr)) return false;
    
    cmd = "AT+CHTTPSPARA=\"USERDATA\",\"X-Filename: " + String(filename) + "\"";
    if (!sendATCommand(cmd.c_str(), 5000, "OK", res, nullptr)) return false;
    
    cmd = "AT+CHTTPSPARA=\"USERDATA\",\"X-Chunk-Offset: " + String(offset) + "\"";
    if (!sendATCommand(cmd.c_str(), 5000, "OK", res, nullptr)) return false;

    cmd = "AT+CHTTPSPARA=\"USERDATA\",\"X-Chunk-Size: " + String(chunkSize) + "\"";
    if (!sendATCommand(cmd.c_str(), 5000, "OK", res, nullptr)) return false;

    cmd = "AT+CHTTPSPARA=\"USERDATA\",\"X-Total-Size: " + String(totalSize) + "\"";
    if (!sendATCommand(cmd.c_str(), 5000, "OK", res, nullptr)) return false;

    cmd = "AT+CHTTPSPARA=\"USERDATA\",\"Content-Length: " + String(chunkSize) + "\"";
    if (!sendATCommand(cmd.c_str(), 5000, "OK", res, nullptr)) return false;
    
    return true;
}


/**
 * Closes the HTTPS session gracefully.
 */
void closeHttpsSession() {
  String res;
  sendATCommand(F("AT+CHTTPSCLSE"), 5000, "+CHTTPSCLSE: 0", res, nullptr);
  sendATCommand(F("AT+CHTTPSSTOP"), 5000, "+CHTTPSSTOP: 0", res, nullptr);
}

/**
 * Uploads a file from the SD card in chunks.
 */
void uploadFile(const char* filename) {
  File file = SD.open(filename);
  if (!file) {
    Serial.println(F("? Failed to open file for upload."));
    return;
  }

  size_t totalSize = file.size();
  Serial.printf("? Preparing to upload %s (%d bytes)\n", filename, totalSize);

  for (size_t offset = 0; offset < totalSize; offset += CHUNK_SIZE) {
    size_t chunkSize = min((size_t)CHUNK_SIZE, totalSize - offset);
    bool chunkSuccess = false;
    for (int attempt = 1; attempt <= 3; ++attempt) {
      Serial.printf("  Attempt %d/3 to send chunk at offset %d...\n", attempt, offset);
      if (sendChunk(file, offset, chunkSize)) {
        chunkSuccess = true;
        break;
      }
      Serial.println(F("? Retrying chunk in 5 seconds..."));
      delay(5000);
    }
    if (!chunkSuccess) {
      Serial.println(F("? Failed to upload chunk after 3 attempts. Aborting."));
      file.close();
      return;
    }
  }

  Serial.println(F("? File upload complete."));
  file.close();
}


/**
 * Sends a single chunk of the file.
 */
bool sendChunk(File& file, size_t offset, size_t chunkSize) {
    String response;

    if (!openHttpsSession()) {
        closeHttpsSession(); // Attempt cleanup
        return false;
    }

    if (!setRequestHeaders(file.name(), file.size(), offset, chunkSize)) {
        Serial.println(F("? Failed to set request headers."));
        closeHttpsSession();
        return false;
    }

    Serial.printf("? Uploading chunk at offset %d (%d bytes)...\n", offset, chunkSize);
    
    // Command to indicate we are about to send data
    String send_cmd = "AT+HTTPSSEND=" + String(chunkSize);
    if (!sendATCommand(send_cmd.c_str(), 5000, ">", response, nullptr)) {
        Serial.println(F("? Failed to initiate data send."));
        closeHttpsSession();
        return false;
    }
    
    // Read chunk from SD and send it directly
    uint8_t buffer[256];
    file.seek(offset);
    for(size_t i = 0; i < chunkSize; i += sizeof(buffer)) {
        size_t readBytes = file.read(buffer, min(sizeof(buffer), chunkSize - i));
        XCOM.write(buffer, readBytes);
    }

    // Wait for the server's response after sending data
    if (!sendATCommand((const char*)nullptr, 20000, "+HTTPSSEND: 0", response, "OK")) {
        Serial.println("? Chunk upload failed. Server response:");
        Serial.println(response);
        closeHttpsSession();
        return false;
    }

    Serial.println(F("? Chunk sent successfully."));
    closeHttpsSession();
    return true;
}


// =================================================================
// Utility and Debug Functions
// =================================================================

/**
 * A robust function to send an AT command and wait for an expected response.
 * Handles reading the full response from the modem.
 */
bool sendATCommand(const char* cmd, unsigned long timeout, const char* expect, String& response, const char* end_marker = "OK") {
    response = "";
    if (cmd) {
        Serial.print(F("[AT SEND] "));
        Serial.println(cmd);
        XCOM.println(cmd);
    }
    
    unsigned long start = millis();
    while (millis() - start < timeout) {
        while (XCOM.available()) {
            char c = XCOM.read();
            response += c;
        }
        if (expect && response.indexOf(expect) != -1) {
            // Also wait for the end marker if specified
            if (end_marker) {
                 if (response.indexOf(end_marker) != -1) {
                    break;
                 }
            } else {
                break;
            }
        }
         // If we don't expect anything, just wait for OK or ERROR
        if (!expect && end_marker && response.indexOf(end_marker) !=-1) {
            break;
        }
    }

    Serial.print(F("[AT RECV] "));
    if(response.length() > 0) {
      Serial.println(response);
    } else {
      Serial.println(F("TIMEOUT"));
    }
    
    if (expect) {
        return response.indexOf(expect) != -1;
    }
    return response.indexOf(end_marker) != -1;
}

// Overload for Flash strings to save memory
bool sendATCommand(const __FlashStringHelper* cmd, unsigned long timeout, const char* expect, String& response, const char* end_marker = "OK") {
    response = "";
    if (cmd) {
        Serial.print(F("[AT SEND] "));
        Serial.println(cmd);
        XCOM.println(cmd);
    }
    
    unsigned long start = millis();
    while (millis() - start < timeout) {
        while (XCOM.available()) {
            char c = XCOM.read();
            response += c;
        }
        if (expect && response.indexOf(expect) != -1) {
            if (end_marker) {
                 if (response.indexOf(end_marker) != -1) {
                    break;
                 }
            } else {
                break;
            }
        }
        if (!expect && end_marker && response.indexOf(end_marker) !=-1) {
            break;
        }
    }
    
    Serial.print(F("[AT RECV] "));
    if(response.length() > 0) {
      Serial.println(response);
    } else {
      Serial.println(F("TIMEOUT"));
    }
    
    if (expect) {
        return response.indexOf(expect) != -1;
    }
    return response.indexOf(end_marker) != -1;
}


/**
 * Prints key modem status details to the Serial monitor.
 */
void printModemStatus() {
  String res;
  Serial.println(F("--- Modem Status ---"));
  
  sendATCommand(F("AT+GSN"), 1000, "OK", res, nullptr);
  res.trim();
  Serial.println("IMEI: " + res.substring(0, res.indexOf("OK")));

  sendATCommand(F("AT+CSQ"), 1000, "OK", res, nullptr);
  res.trim();
  res.remove(0, res.indexOf(':') + 2);
  Serial.println("Signal Quality: " + res.substring(0, res.indexOf(',')));

  sendATCommand(F("AT+CPIN?"), 1000, "OK", res, nullptr);
  res.trim();
  res.remove(0, res.indexOf(':') + 2);
  Serial.println("SIM Status: " + res.substring(0, res.indexOf('\r')));
  
  sendATCommand(F("AT+CCID"), 1000, "OK", res, nullptr);
  res.trim();
  res.remove(0, res.indexOf(':') + 2);
  Serial.println("CCID: " + res.substring(0, res.indexOf('\r')));

  sendATCommand(F("AT+COPS?"), 1000, "OK", res, nullptr);
  res.trim();
  res.remove(0, res.indexOf('"') + 1);
  Serial.println("Operator: " + res.substring(0, res.lastIndexOf('"')));
  
  Serial.println(F("--------------------"));
}
