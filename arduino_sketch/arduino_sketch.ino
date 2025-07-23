// Define the serial connections for the modem
#define MODEM_RX 16
#define MODEM_TX 17
#define MODEM_BAUD 115200

// Define the SD card CS pin
#define SD_CS_PIN 5

// Your GPRS credentials
const char apn[]      = "internet"; // APN for One NZ. Change if you use a different provider.
const char gprsUser[] = "";      // GPRS User, usually blank
const char gprsPass[] = "";      // GPRS Password, usually blank

// Server details
const char server[] = "cellular-data-streamer.web.app";
const int port = 443;
const char* resource = "/api/upload";

// Libraries
#include <SD.h>
#include <SPI.h>
#include <HardwareSerial.h>

#define TINY_GSM_MODEM_SIM7600
#include <TinyGsmClient.h>

// Globals
HardwareSerial modemSerial(1);
TinyGsm modem(modemSerial);

// Function Prototypes
void listDir(fs::FS &fs, const char * dirname, uint8_t levels);
void sendFile(const char* filePath);
bool sendChunk(File& file, const char* filename, size_t offset, size_t totalSize);
bool openHttpsSession();
void closeHttpsSession();
bool configureSSL();
bool waitForModemReady();
bool waitForNetwork();
void sendATCommand(const char* cmd, unsigned long timeout, const char* expected_response);
bool sendATCommandCheck(const char* cmd, unsigned long timeout = 5000, const char* expected1 = "OK", const char* expected2 = "");


void setup() {
  Serial.begin(115200);
  while (!Serial);

  Serial.println("? Booting...");

  SPI.begin();
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("? SD Card Mount Failed");
    return;
  }
  uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("? No SD card attached");
    return;
  }
  Serial.println("? SD card ready.");

  Serial.println("Initializing modem...");
  modemSerial.begin(MODEM_BAUD, SERIAL_8N1, MODEM_TX, MODEM_RX);
  
  if (!modem.restart()) {
    Serial.println("? Modem restart failed.");
    return;
  }
  Serial.println("? Modem restarted.");
  
  Serial.println("--- Modem Status ---");
  Serial.println("Modem Info: " + modem.getModemInfo());
  Serial.println("Signal Quality: " + String(modem.getSignalQuality()));
  Serial.println("SIM Status: " + String(modem.getSimStatus()));
  Serial.println("CCID: " + modem.getSimCCID());
  Serial.println("Operator: " + modem.getOperator());
  Serial.println("--------------------");

  if (!waitForModemReady()) {
    Serial.println("? Modem not ready. Halting.");
    while(true);
  }
  Serial.println("? Modem is ready.");

  if (!waitForNetwork()) {
      Serial.println("? Network registration failed. Halting.");
      while(true);
  }
  Serial.println("? Registered on network.");

  Serial.println("? Connecting to network...");
  if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
      Serial.println("? GPRS connection failed.");
      return;
  }
  Serial.println("? GPRS connected.");
  Serial.println("Local IP: " + modem.getLocalIP());

  if (!configureSSL()) {
    Serial.println("? Failed to configure SSL context.");
    return;
  }
  Serial.println("? SSL Context configured.");

  // List all files on the SD card
  listDir(SD, "/", 0);
  
  // Pick a file to send. Replace with your logic to select a file.
  const char* fileToSend = "/sigma2.wav"; 
  
  File file = SD.open(fileToSend);
  if (file) {
    sendFile(fileToSend);
    file.close();
  } else {
    Serial.println("? Failed to open file for sending.");
  }
}

void loop() {
  // Keep the main loop clean. The setup handles the one-time upload.
  delay(10000);
}


// Function to send a single data chunk
bool sendChunk(File& file, const char* filename, size_t offset, size_t totalSize) {
    uint8_t chunkBuffer[4096];
    size_t chunkSize = file.read(chunkBuffer, sizeof(chunkBuffer));

    if (chunkSize == 0) {
        return false; // No more data to read
    }

    Serial.printf("  Attempting to send chunk at offset %u, size %u...\n", offset, chunkSize);

    // Construct headers
    char headers[256];
    snprintf(headers, sizeof(headers),
             "x-filename: %s\r\nx-chunk-offset: %u\r\nx-chunk-size: %u\r\nx-total-size: %u\r\nContent-Type: application/octet-stream",
             filename, offset, chunkSize, totalSize);
    
    char atCmd[512];
    snprintf(atCmd, sizeof(atCmd), "AT+CHTTPSPOST=\"%s\",%d,%d,%d", resource, strlen(headers), chunkSize, 15000);

    // Send the POST command and wait for the '>' prompt
    modem.sendAT(atCmd);
    if (!modem.waitResponse(5000, ">")) {
        Serial.println("? Modem did not respond to POST command. Aborting.");
        return false;
    }

    // Send headers
    modemSerial.println(headers);
    modemSerial.println(); // Blank line to end headers

    // Send binary chunk data
    modemSerial.write(chunkBuffer, chunkSize);
    modemSerial.flush();

    // Wait for the server response (e.g., "+CHTTPS: POST,200,...")
    String response = "";
    if (modem.waitResponse(15000, &response) != 1) {
        Serial.println("? No response from server after sending chunk.");
        return false;
    }
    
    Serial.println("[SERVER RSP] " + response);
    if (response.indexOf("+CHTTPS: POST,200") != -1 || response.indexOf("+CHTTPS: POST,201") != -1) {
        Serial.println("  ...chunk sent successfully.");
        return true;
    } else {
        Serial.println("  ...server returned an error.");
        return false;
    }
}


// Main function to manage sending a file in chunks
void sendFile(const char* filePath) {
    File file = SD.open(filePath, FILE_READ);
    if (!file) {
        Serial.println("? Failed to open file for sending.");
        return;
    }

    size_t fileSize = file.size();
    const char* filename = file.name();
    Serial.printf("? Preparing to upload %s (%u bytes)\n", filename, fileSize);

    if (!openHttpsSession()) {
        Serial.println("? Failed to open HTTPS session.");
        file.close();
        return;
    }

    size_t offset = 0;
    while (offset < fileSize) {
        bool success = sendChunk(file, filename, offset, fileSize);
        if (success) {
            offset = file.position();
        } else {
            Serial.printf("? Failed to upload chunk at offset %u. Aborting.\n", offset);
            break; // Exit loop on first failure
        }
    }

    closeHttpsSession();

    if (offset >= fileSize) {
        Serial.println("? File upload completed successfully!");
    } else {
        Serial.println("? File upload failed.");
    }

    file.close();
}


bool openHttpsSession() {
    if (!sendATCommandCheck("AT+CHTTPSSTART")) {
        return false;
    }
    if (!sendATCommandCheck("AT+CHTTPSOPSE=\"" + String(server) + "\"," + String(port), 15000, "+CHTTPSOPSE: 0")) {
        Serial.println("? Failed to open HTTPS session with server.");
        closeHttpsSession(); // Attempt cleanup
        return false;
    }
    return true;
}

void closeHttpsSession() {
    sendATCommandCheck("AT+CHTTPSCLSE");
    sendATCommandCheck("AT+CHTTPSSTOP");
}

bool configureSSL() {
  Serial.println("? Configuring SSL...");
  if (!sendATCommandCheck("AT+CSSLCFG=\"sslversion\",1,3")) return false;
  if (!sendATCommandCheck("AT+CSSLCFG=\"authmode\",1,0")) return false;
  return true;
}

// Utility to send an AT command and wait for a specific response
bool sendATCommandCheck(const char* cmd, unsigned long timeout, const char* expected1, const char* expected2) {
    Serial.println("[AT SEND] " + String(cmd));
    modem.sendAT(cmd);
    String response = "";
    if (modem.waitResponse(timeout, &response) != 1) {
        Serial.println("[DEBUG] Timeout waiting for: " + String(expected1));
        Serial.println("[DEBUG] Received: " + response);
        return false;
    }
    response.trim();
    if (response.indexOf(expected1) == -1 && (strlen(expected2) == 0 || response.indexOf(expected2) == -1)) {
        Serial.println("[DEBUG] Unexpected response: " + response);
        return false;
    }
    return true;
}

// Blocks until the modem is fully booted and the SIM is ready.
bool waitForModemReady() {
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

// Blocks until the modem is registered on the cellular network.
bool waitForNetwork() {
  Serial.println("? Waiting for network registration...");
  for (int i = 0; i < 30; i++) { // Wait for up to 30 seconds
    int registrationStatus = modem.getRegistrationStatus();
    // 1: Registered, home network. 5: Registered, roaming.
    if (registrationStatus == 1 || registrationStatus == 5) {
      return true;
    }
    delay(1000);
  }
  return false;
}


// Lists files and directories on the SD card
void listDir(fs::FS &fs, const char * dirname, uint8_t levels) {
    Serial.printf("Listing directory: %s\n", dirname);
    File root = fs.open(dirname);
    if (!root) {
        Serial.println("Failed to open directory");
        return;
    }
    if (!root.isDirectory()) {
        Serial.println("Not a directory");
        return;
    }
    File file = root.openNextFile();
    while (file) {
        if (file.isDirectory()) {
            Serial.print("  DIR : ");
            Serial.println(file.name());
            if (levels) {
                listDir(fs, file.name(), levels - 1);
            }
        } else {
            Serial.print("  FILE: ");
            Serial.print(file.name());
            Serial.print("  SIZE: ");
            Serial.println(file.size());
        }
        file = file.openNextFile();
    }
}
