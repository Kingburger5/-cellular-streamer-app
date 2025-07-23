#include <SPI.h>
#include <SD.h>
#include <HardwareSerial.h>
#include <TinyGsmClient.h>

// Modem pins
#define MODEM_RX_PIN 16
#define MODEM_TX_PIN 17
#define MODEM_BAUD_RATE 115200

// SD card pins
#define SD_CS_PIN 5

// APN settings
const char apn[] = "internet"; // Replace with your APN
const char gprsUser[] = "";
const char gprsPass[] = "";

// Server settings
const char server[] = "6000-firebase-studio-1753223410587.cluster-73qgvk7hjjadkrjeyexca5ivva.cloudworkstations.dev";
const int port = 443;
const char* resource = "/api/upload";

HardwareSerial modemSerial(1);
TinyGsm modem(modemSerial);

// --- Function Prototypes ---
void setupModem();
bool waitForModemReady();
bool waitForNetwork();
void printModemStatus();
void sendFile(const char* filename);
bool sendChunk(File& file, const char* filename, size_t offset, size_t totalSize);
bool openHttpsSession();
void closeHttpsSession();

void setup() {
  Serial.begin(115200);
  while (!Serial);

  Serial.println("\n\U0001F50C Booting...");

  // Initialize SD card
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("\u274C SD card initialization failed!");
    while (1);
  }
  Serial.println("\U0001F4BE SD card ready.");
  
  // Setup and connect modem
  setupModem();

  // Send a file from the SD card
  sendFile("/sigma2.wav"); // Change this to your file's name

  Serial.println("\U0001F4A1 Task finished. Entering idle loop.");
}

void loop() {
  // Idle loop
  delay(10000);
}

void setupModem() {
  Serial.println("Initializing modem...");
  modemSerial.begin(MODEM_BAUD_RATE, SERIAL_8N1, MODEM_RX_PIN, MODEM_TX_PIN);
  
  delay(1000);
  if (!modem.restart()) {
    Serial.println("\u274C Failed to restart modem. Halting.");
    while(1);
  }
  Serial.println("\u2753 Modem restarted.");

  Serial.println("Waiting for modem to be ready...");
  if (!waitForModemReady()) {
    Serial.println("\u274C Modem not ready. Halting.");
    while (1);
  }
  Serial.println("\u2753 Modem and SIM are ready.");

  printModemStatus();

  Serial.println("--------------------");
  Serial.println("\U0001F4E1 Connecting to network...");

  if (!waitForNetwork()) {
      Serial.println("\u274C Failed to register on network. Halting.");
      while(1);
  }
  
  if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
    Serial.println("\u274C GPRS connection failed.");
  } else {
    Serial.println("\u2705 GPRS connected.");
    Serial.print("Local IP: ");
    Serial.println(modem.getLocalIP());
  }
}

bool waitForModemReady() {
  for (int i = 0; i < 20; i++) {
    if (modem.testAT(1000)) {
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
  Serial.println("Waiting for network registration...");
  for (int i = 0; i < 30; i++) {
    int registrationStatus = modem.getRegistrationStatus();
    Serial.print("Network registration status: ");
    Serial.println(registrationStatus);
    if (registrationStatus == 1 || registrationStatus == 5) {
      Serial.println("\u2753 Registered on network.");
      return true;
    }
    delay(1000);
  }
  return false;
}

void printModemStatus() {
  Serial.println("--- Modem Status ---");
  String imei = modem.getIMEI();
  Serial.println("IMEI: " + imei);

  int csq = modem.getSignalQuality();
  Serial.println("Signal Quality: " + String(csq));
  
  int simStatus = modem.getSimStatus();
  Serial.println("SIM Status: " + String(simStatus));

  String ccid = modem.getSimCCID();
  Serial.println("CCID: " + ccid);

  String op = modem.getOperator();
  Serial.println("Operator: " + op);
  Serial.println("--------------------");
}

void sendFile(const char* filename) {
  File file = SD.open(filename, FILE_READ);
  if (!file) {
    Serial.println("\u274C Failed to open file: " + String(filename));
    return;
  }

  size_t fileSize = file.size();
  Serial.println("\U0001F4E4 Preparing to upload " + String(filename) + " (" + String(fileSize) + " bytes)");

  size_t offset = 0;
  while (offset < fileSize) {
    bool success = false;
    for (int i = 0; i < 3; i++) {
      Serial.println("  Attempt " + String(i + 1) + "/3 to send chunk at offset " + String(offset) + "...");
      if (sendChunk(file, filename, offset, fileSize)) {
        success = true;
        break;
      }
      Serial.println("    ...connection failed.");
      file.seek(offset); // Rewind file position for retry
    }

    if (!success) {
      Serial.println("\u2753 Failed to upload chunk at offset " + String(offset) + " after 3 retries. Aborting.");
      file.close();
      return;
    }
    offset = file.position();
  }

  Serial.println("\U0001F3C1 File upload complete.");
  file.close();
}

bool sendChunk(File& file, const char* filename, size_t offset, size_t totalSize) {
  if (!openHttpsSession()) {
    closeHttpsSession(); // Ensure cleanup on failure
    return false;
  }

  const int CHUNK_BUFFER_SIZE = 4096;
  uint8_t chunkBuffer[CHUNK_BUFFER_SIZE];
  
  size_t chunkSize = file.read(chunkBuffer, CHUNK_BUFFER_SIZE);
  if (chunkSize == 0) {
    Serial.println("  [DEBUG] Read 0 bytes from file. End of file?");
    closeHttpsSession();
    return true; // Nothing more to read
  }

  String headers = "X-Filename: " + String(filename) + "\r\n" +
                   "X-Chunk-Offset: " + String(offset) + "\r\n" +
                   "X-Chunk-Size: " + String(chunkSize) + "\r\n" +
                   "X-Total-Size: " + String(totalSize) + "\r\n" +
                   "Content-Type: application/octet-stream";

  String cmd = "AT+CHTTPSPOST=\"" + String(resource) + "\"," + String(headers.length()) + "," + String(10000) + "," + String(chunkSize);
  
  modem.sendAT(cmd);
  if (!modem.waitResponse(10000, ">")) {
    Serial.println("  [DEBUG] Modem did not respond to POST command with '>'. Aborting.");
    closeHttpsSession();
    return false;
  }
  
  modem.sendAT(headers);
  modem.waitResponse(100, "\r\n");

  modemSerial.write(chunkBuffer, chunkSize);
  
  String response;
  modem.waitResponse(20000, response);
  
  Serial.println("  [DEBUG] Server response: " + response);

  closeHttpsSession();

  if (response.indexOf("200 OK") != -1 || response.indexOf("201") != -1) {
    Serial.println("  \u2705 Chunk uploaded successfully.");
    return true;
  } else {
    Serial.println("  \u274C Chunk upload failed with unexpected response.");
    return false;
  }
}

bool openHttpsSession() {
  modem.sendAT(GF("AT+CHTTPSSTART"));
  if (modem.waitResponse(5000) != 1) {
    Serial.println("  [DEBUG] Failed on AT+CHTTPSSTART");
    return false;
  }
  
  String cmd = "AT+CHTTPSOPSE=\"" + String(server) + "\"," + String(port);
  modem.sendAT(cmd);
  if (modem.waitResponse(20000, "+CHTTPSOPSE: 0") != 1) {
     Serial.println("  [DEBUG] Failed to open HTTPS session with server.");
     return false;
  }

  Serial.println("  \u2705 HTTPS session opened.");
  return true;
}

void closeHttpsSession() {
  modem.sendAT(GF("AT+CHTTPSCLSE"));
  modem.waitResponse(5000);
  modem.sendAT(GF("AT+CHTTPSSTOP"));
  modem.waitResponse(5000);
  Serial.println("  [DEBUG] HTTPS Session closed.");
}