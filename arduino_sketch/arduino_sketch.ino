// Define modem type
#define TINY_GSM_MODEM_SIM7600
// Set serial for debug prints
#define SerialMon Serial
// Set serial for AT commands (to the modem)
#define XCOM Serial1

#include <Arduino.h>
#include <HardwareSerial.h>
#include <SD.h>
#include <SPI.h>

// Modem connection details
#define MODEM_BAUD 115200
#define MODEM_RX 16
#define MODEM_TX 17

// SD card pin
#define SD_CS 5

// Upload settings
const char apn[] = "internet";
const char server[] = "6000-firebase-studio-1753223410587.cluster-73qgvk7hjjadkrjeyexca5ivva.cloudworkstations.dev";
const char endpoint[] = "/api/upload";
const int port = 443;
#define CHUNK_SIZE 4096

// Forward declarations for helper functions to prevent compilation errors
bool sendATCommand(const char* cmd, unsigned long timeout, const char* expect, const char* expect2 = nullptr, const char* expect3 = nullptr);
bool sendATCommand(const __FlashStringHelper* cmd, unsigned long timeout, const char* expect, const char* expect2 = nullptr, const char* expect3 = nullptr);
bool sendATCommand(const char* cmd, unsigned long timeout, const char* expect, String& response, const char* end_marker = "\r\n");
bool sendATCommand(const __FlashStringHelper* cmd, unsigned long timeout, const char* expect, String& response, const char* end_marker = "\r\n");

// Enum for network registration status
enum RegStatus {
  NOT_REGISTERED = 0,
  REGISTERED_HOME = 1,
  SEARCHING = 2,
  REGISTRATION_DENIED = 3,
  UNKNOWN = 4,
  REGISTERED_ROAMING = 5
};

void setup() {
  SerialMon.begin(115200);
  delay(1000);

  SerialMon.println(F("? Booting..."));

  if (!SD.begin(SD_CS)) {
    SerialMon.println(F("? SD card failed."));
    while (true);
  }
  SerialMon.println(F("? SD card ready."));

  SerialMon.println(F("? Initializing modem..."));
  XCOM.begin(MODEM_BAUD, SERIAL_8N1, MODEM_RX, MODEM_TX);
  delay(3000);

  // Wait for modem to be ready
  SerialMon.println(F("? Waiting for modem to be ready..."));
  while (!sendATCommand(F("AT"), 1000, "OK")) {
    delay(1000);
  }
  SerialMon.println(F("? Modem is ready."));

  printModemStatus();

  // Wait for network registration
  SerialMon.println(F("? Waiting for network registration..."));
  while (getRegistrationStatus() != REGISTERED_HOME) {
    SerialMon.println("? Network registration status: " + String((int)getRegistrationStatus()));
    delay(2000);
  }
  SerialMon.println(F("? Registered on network."));

  if (!manualGprsConnect()) {
    SerialMon.println(F("? GPRS connection failed. Halting."));
    while (true);
  }

  // Upload your file here
  uploadFile("/sigma2.wav");
}

void loop() {
  // Nothing here
}

void printModemStatus() {
  String res;
  SerialMon.println(F("--- Modem Status ---"));
  if (sendATCommand(F("AT+GSN"), 1000, "OK", res)) {
    res.trim();
    SerialMon.println("IMEI: " + res);
  }
  if (sendATCommand(F("AT+CSQ"), 1000, "OK", res)) {
    int rssi = res.substring(res.indexOf(':') + 2, res.indexOf(',')).toInt();
    SerialMon.println("Signal Quality: " + String(rssi));
  }
  if (sendATCommand(F("AT+CPIN?"), 1000, "READY", res)) {
    SerialMon.println("SIM Status: 1");
  } else {
    SerialMon.println("SIM Status: 0");
  }
  if (sendATCommand(F("AT+CCID"), 1000, "OK", res)) {
    res.trim();
    SerialMon.println("CCID: " + res);
  }
  if (sendATCommand(F("AT+COPS?"), 1000, "OK", res)) {
    int first_quote = res.indexOf('"') + 1;
    int second_quote = res.indexOf('"', first_quote);
    String operatorName = res.substring(first_quote, second_quote);
    SerialMon.println("Operator: " + operatorName);
  }
  SerialMon.println(F("--------------------"));
}

RegStatus getRegistrationStatus() {
  String res;
  if (sendATCommand(F("AT+CREG?"), 1000, "OK", res)) {
    int status = res.substring(res.lastIndexOf(',') + 1).toInt();
    return (RegStatus)status;
  }
  return UNKNOWN;
}

bool manualGprsConnect() {
  SerialMon.println(F("? Connecting to GPRS..."));
  if (!sendATCommand(F("AT+CGDCONT=1,\"IP\",\"internet\""), 10000, "OK")) return false;
  if (!sendATCommand(F("AT+CGACT=1,1"), 20000, "OK")) {
    SerialMon.println(F("? Failed to activate GPRS context."));
    return false;
  }
  String ip_addr;
  if (sendATCommand(F("AT+CGPADDR=1"), 10000, "OK", ip_addr) && ip_addr.indexOf("1,") != -1) {
    ip_addr = ip_addr.substring(ip_addr.indexOf("1,") + 2);
    ip_addr.trim();
    SerialMon.println("? GPRS Connected. IP: " + ip_addr);
    return true;
  }
  return false;
}

bool openHttpsSession() {
  if (!sendATCommand(F("AT+CHTTPSSTART"), 20000, "OK")) return false;

  String cmd = "AT+CHTTPSOPSE=\"";
  cmd += server;
  cmd += "\",";
  cmd += port;
  
  if (!sendATCommand(cmd.c_str(), 30000, "+CHTTPSOPSE: 0")) {
      SerialMon.println(F("? Failed to open HTTPS session."));
      closeHttpsSession();
      return false;
  }

  cmd = "AT+CHTTPSPARA=\"URL\",\"";
  cmd += endpoint;
  cmd += "\"";
  if (!sendATCommand(cmd.c_str(), 20000, "OK")) {
    SerialMon.println(F("? Failed to set URL parameter."));
    closeHttpsSession();
    return false;
  }
  
  return true;
}

bool setRequestHeaders(const char* filename, size_t offset, size_t chunkSize, size_t totalSize) {
    String cmd;
    
    cmd = "AT+CHTTPSPARA=\"USERDATA\",\"Content-Type: application/octet-stream\"";
    if (!sendATCommand(cmd.c_str(), 5000, "OK")) return false;

    cmd = "AT+CHTTPSPARA=\"USERDATA\",\"X-Filename: ";
    cmd += filename;
    cmd += "\"";
    if (!sendATCommand(cmd.c_str(), 5000, "OK")) return false;

    cmd = "AT+CHTTPSPARA=\"USERDATA\",\"X-Chunk-Offset: ";
    cmd += offset;
    cmd += "\"";
    if (!sendATCommand(cmd.c_str(), 5000, "OK")) return false;

    cmd = "AT+CHTTPSPARA=\"USERDATA\",\"X-Chunk-Size: ";
    cmd += chunkSize;
    cmd += "\"";
    if (!sendATCommand(cmd.c_str(), 5000, "OK")) return false;

    cmd = "AT+CHTTPSPARA=\"USERDATA\",\"X-Total-Size: ";
    cmd += totalSize;
    cmd += "\"";
    if (!sendATCommand(cmd.c_str(), 5000, "OK")) return false;
    
    cmd = "AT+CHTTPSPARA=\"CONTENT-LENGTH\",";
    cmd += chunkSize;
    if (!sendATCommand(cmd.c_str(), 5000, "OK")) return false;

    return true;
}

void closeHttpsSession() {
    sendATCommand(F("AT+CHTTPSCLSE"), 5000, "OK");
    sendATCommand(F("AT+CHTTPSSTOP"), 5000, "OK");
}

void uploadFile(const char* filename) {
  File file = SD.open(filename);
  if (!file) {
    SerialMon.println("? Failed to open file.");
    return;
  }

  size_t totalSize = file.size();
  SerialMon.println("? Preparing to upload " + String(filename) + " (" + String(totalSize) + " bytes)");

  size_t offset = 0;
  while (offset < totalSize) {
    bool chunk_success = false;
    for (int attempt = 1; attempt <= 3; attempt++) {
      SerialMon.println("  Attempt " + String(attempt) + "/3 to send chunk at offset " + String(offset) + "...");
      if (sendChunk(file, offset, totalSize)) {
        chunk_success = true;
        break;
      }
      SerialMon.println("  Retrying chunk in 5 seconds...");
      delay(5000);
    }
    if (chunk_success) {
      offset += CHUNK_SIZE;
    } else {
      SerialMon.println("? Failed to upload chunk after 3 attempts. Aborting.");
      break;
    }
  }

  if (offset >= totalSize) {
    SerialMon.println(F("? File upload completed successfully."));
  }
  file.close();
}

bool sendChunk(File& file, size_t offset, size_t totalSize) {
  if (!openHttpsSession()) {
    return false;
  }
  
  size_t chunkSize = min((size_t)CHUNK_SIZE, totalSize - offset);
  if (!setRequestHeaders(file.name(), offset, chunkSize, totalSize)) {
      SerialMon.println(F("? Failed to set request headers."));
      closeHttpsSession();
      return false;
  }

  String cmd = "AT+CHTTPSD=";
  cmd += chunkSize;
  cmd += ",60000"; // 60s timeout for download prompt
  String response;
  if (!sendATCommand(cmd.c_str(), 5000, "DOWNLOAD", response)) {
    SerialMon.println(F("? Modem did not prompt for data."));
    closeHttpsSession();
    return false;
  }

  // Send the file data
  file.seek(offset);
  uint8_t buffer[256];
  size_t bytes_sent = 0;
  while(bytes_sent < chunkSize) {
      size_t to_read = min((size_t)sizeof(buffer), chunkSize - bytes_sent);
      size_t bytes_read = file.read(buffer, to_read);
      if (bytes_read == 0) break;
      XCOM.write(buffer, bytes_read);
      bytes_sent += bytes_read;
  }
  XCOM.flush();

  // Wait for server response after data sent
  if (!sendATCommand((const char*)nullptr, 60000, "+CHTTPS: 200")) {
    SerialMon.println("? Chunk upload failed. Server response did not contain 200 OK.");
    closeHttpsSession();
    return false;
  }
  
  SerialMon.println("? Chunk successfully uploaded.");
  closeHttpsSession();
  return true;
}

// =================================================================
// Generic AT command helper function (with overloads)
// =================================================================

bool sendATCommand(const char* cmd, unsigned long timeout, const char* expect, String& response, const char* end_marker) {
    if (cmd) {
        SerialMon.println("[AT SEND] " + String(cmd));
        XCOM.println(cmd);
    }
    
    response = "";
    unsigned long start = millis();
    while (millis() - start < timeout) {
        while (XCOM.available()) {
            char c = XCOM.read();
            response += c;
        }
        if (response.indexOf(expect) != -1) {
            if (end_marker == nullptr || response.indexOf(end_marker) != -1) {
                 SerialMon.print("[AT RECV] " + response);
                 return true;
            }
        }
    }
    SerialMon.println("[AT RECV TIMEOUT] " + response);
    return false;
}

bool sendATCommand(const char* cmd, unsigned long timeout, const char* expect, const char* expect2, const char* expect3) {
    String response;
    if (cmd) {
        SerialMon.println("[AT SEND] " + String(cmd));
        XCOM.println(cmd);
    }
    unsigned long start = millis();
    while (millis() - start < timeout) {
        if (XCOM.available()) {
            response += XCOM.readString();
        }
        if (response.indexOf(expect) != -1) {
             SerialMon.print("[AT RECV] " + response);
            return true;
        }
        if (expect2 && response.indexOf(expect2) != -1) {
             SerialMon.print("[AT RECV] " + response);
            return true;
        }
        if (expect3 && response.indexOf(expect3) != -1) {
             SerialMon.print("[AT RECV] " + response);
            return true;
        }
    }
    SerialMon.println("[AT RECV TIMEOUT] " + response);
    return false;
}

// Overload for Flash-based strings
bool sendATCommand(const __FlashStringHelper* cmd, unsigned long timeout, const char* expect, String& response, const char* end_marker) {
    return sendATCommand((const char*)cmd, timeout, expect, response, end_marker);
}

bool sendATCommand(const __FlashStringHelper* cmd, unsigned long timeout, const char* expect, const char* expect2, const char* expect3) {
    return sendATCommand((const char*)cmd, timeout, expect, expect2, expect3);
}
