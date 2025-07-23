
#define TINY_GSM_MODEM_SIM7600
#define MODEM_TX_PIN 17
#define MODEM_RX_PIN 16
#define SD_CS_PIN 5

#include <SD.h>
#include <SPI.h>
#include <HardwareSerial.h>

#define SerialMon Serial
#define TINY_GSM_DEBUG SerialMon

#include <TinyGsmClient.h>

const char server[] = "6000-firebase-studio-1753223410587.cluster-73qgvk7hjjadkrjeyexca5ivva.cloudworkstations.dev";
const char resource[] = "/api/upload";
const int port = 443;
const char apn[] = "internet";
const char gprsUser[] = "";
const char gprsPass[] = "";

HardwareSerial modemSerial(1);
TinyGsm modem(modemSerial);

bool sendATCommand(const char* cmd, unsigned long timeout, const char* expectedResponse) {
  modemSerial.println(cmd);
  String response = "";
  unsigned long startTime = millis();
  while (millis() - startTime < timeout) {
    if (modemSerial.available()) {
      char c = modemSerial.read();
      response += c;
      if (response.endsWith(expectedResponse)) {
        return true;
      }
    }
  }
  return false;
}

bool sendATCommandCheck(const char* cmd, unsigned long timeout, const char* okResp, const char* errResp) {
    modem.sendAT(cmd);
    String res;
    if (modem.waitResponse(timeout, res) != 1) {
        return false;
    }
    return res.indexOf(okResp) != -1;
}


bool setupModem() {
  SerialMon.println("Initializing modem...");

  // Use the correct pins for your board
  modemSerial.begin(115200, SERIAL_8N1, MODEM_RX_PIN, MODEM_TX_PIN);

  if (!modem.restart()) {
    SerialMon.println("? Modem restart failed.");
    return false;
  }
  SerialMon.println("? Modem restarted.");
  return true;
}

bool waitForModemReady() {
  SerialMon.println("Waiting for modem to be ready...");
  for (int i = 0; i < 20; i++) {
    if (modem.testAT()) {
      SimStatus simStatus = modem.getSimStatus();
      if (simStatus == SIM_READY) {
        return true;
      }
    }
    delay(500);
  }
  return false;
}

bool waitForNetwork() {
  SerialMon.println("Waiting for network registration...");
  int i = 0;
  while (i < 30) {
      int registrationStatus = modem.getRegistrationStatus();
      SerialMon.print("Network registration status: ");
      SerialMon.println(registrationStatus);
      if (registrationStatus == 1 || registrationStatus == 5) { // 1: Registered, home network; 5: Registered, roaming
          return true;
      }
      delay(1000);
      i++;
  }
  return false;
}

void printModemStatus() {
  SerialMon.println("--- Modem Status ---");
  String imei = modem.getIMEI();
  SerialMon.println("IMEI: " + imei);

  int csq = modem.getSignalQuality();
  SerialMon.println("Signal Quality: " + String(csq));
  
  SimStatus simStatus = modem.getSimStatus();
  SerialMon.println("SIM Status: " + String(simStatus));

  String ccid = modem.getSimCCID();
  SerialMon.println("CCID: " + ccid);

  String op = modem.getOperator();
  SerialMon.println("Operator: " + op);
  SerialMon.println("--------------------");
}


bool openHttpsSession() {
  if (!sendATCommandCheck("AT+CHTTPSSTART", 1000, "OK", "ERROR")) {
    return false;
  }
  String cmd = "AT+CHTTPSOPSE=\"" + String(server) + "\"," + String(port);
  if (!sendATCommandCheck(cmd.c_str(), 20000, "+CHTTPSOPSE: 0", "ERROR")) {
    return false;
  }
  return true;
}

void closeHttpsSession() {
    sendATCommandCheck("AT+CHTTPSCLSE", 1000, "OK", "ERROR");
    sendATCommandCheck("AT+CHTTPSSTOP", 1000, "OK", "ERROR");
}

bool sendChunk(File& file, const char* filename, size_t offset, size_t totalSize) {
    const int MAX_RETRIES = 3;
    for (int attempt = 1; attempt <= MAX_RETRIES; ++attempt) {
        SerialMon.println("  Attempt " + String(attempt) + "/" + String(MAX_RETRIES) + " to send chunk at offset " + String(offset) + "...");
        
        if (!openHttpsSession()) {
            SerialMon.println("    ...connection failed.");
            closeHttpsSession(); // Ensure cleanup on failure
            delay(2000); // Wait before retrying
            continue;
        }

        uint8_t chunkBuffer[4096];
        size_t chunkSize = file.read(chunkBuffer, sizeof(chunkBuffer));
        if (chunkSize == 0) {
            SerialMon.println("    ...read 0 bytes, assuming end of file.");
            closeHttpsSession();
            return true; // Finished reading
        }
        
        String headers = "x-filename: " + String(filename) + "\r\n"
                       + "x-chunk-offset: " + String(offset) + "\r\n"
                       + "x-chunk-size: " + String(chunkSize) + "\r\n"
                       + "x-total-size: " + String(totalSize) + "\r\n"
                       + "Content-Type: application/octet-stream";

        String postCmd = "AT+CHTTPSPOST=\"" + String(resource) + "\"," + String(headers.length()) + "," + String(chunkSize);
        if (!sendATCommandCheck(postCmd.c_str(), 1000, ">", "ERROR")) {
            SerialMon.println("    ...POST command failed.");
            closeHttpsSession();
            delay(2000);
            continue;
        }

        modemSerial.println(headers);
        modemSerial.write(chunkBuffer, chunkSize);
        
        String res;
        if (modem.waitResponse(20000, res) != 1 || res.indexOf("OK") == -1) {
            SerialMon.println("    ...upload failed.");
            closeHttpsSession();
            delay(2000);
            continue;
        }

        SerialMon.println("    ...chunk uploaded successfully.");
        closeHttpsSession();
        return true;
    }

    SerialMon.println("? Failed to upload chunk at offset " + String(offset) + " after " + String(MAX_RETRIES) + " retries. Aborting.");
    return false;
}

void sendFileInChunks(const char* filename) {
  File file = SD.open(filename);
  if (!file) {
    SerialMon.println("? Failed to open file for reading");
    return;
  }

  size_t totalSize = file.size();
  SerialMon.println("? Preparing to upload " + String(filename) + " (" + String(totalSize) + " bytes)");

  size_t offset = 0;
  while (offset < totalSize) {
    file.seek(offset); // Position the file for the next chunk read
    if (!sendChunk(file, filename, offset, totalSize)) {
      break; 
    }
    // sendChunk reads 4096 bytes, so we advance by that amount
    offset += 4096;
  }

  file.close();
}


void setup() {
  SerialMon.begin(115200);
  while (!SerialMon);

  SerialMon.println("? Booting...");

  if (!SD.begin(SD_CS_PIN)) {
    SerialMon.println("? SD card initialization failed!");
    while (1);
  }
  SerialMon.println("? SD card ready.");

  if (!setupModem() || !waitForModemReady()) {
    SerialMon.println("? Modem initialization failed. Halting.");
    while (1);
  }

  SerialMon.println("? Modem and SIM are ready.");
  printModemStatus();
  
  SerialMon.println("? Connecting to network...");
  if (!waitForNetwork()) {
      SerialMon.println("? Failed to register on network. Halting.");
      while(1);
  }
  SerialMon.println("? Registered on network.");

  if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
      SerialMon.println("? GPRS connection failed.");
      while(1);
  }
  SerialMon.println("? GPRS connected.");

  String ip = modem.getLocalIP();
  SerialMon.println("Local IP: " + ip);

  sendFileInChunks("/sigma2.wav");
  SerialMon.println("? Task finished. Entering idle loop.");
}

void loop() {
  // Idle loop to keep the device running
  delay(10000);
}
