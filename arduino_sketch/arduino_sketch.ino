// Define the serial port for the modem
#define MODEM_RX 16
#define MODEM_TX 17
#define MODEM_BAUD 115200
#define SD_CS_PIN 5

// Libraries
#include <HardwareSerial.h>
#include <SD.h>
#include <TinyGsmClient.h>

// Modem & SD Card setup
HardwareSerial modemSerial(1);
TinyGsm modem(modemSerial);

// Server configuration
const char server[] = "cellular-data-streamer.web.app";
const char resource[] = "/api/upload";
const int port = 443;
const char apn[] = "internet"; // Your APN
const char gprsUser[] = "";    // GPRS User, if required
const char gprsPass[] = "";    // GPRS Password, if required

// File to upload
const char* filename = "/sigma2.wav";

// --- Utility Functions ---

// Function to print debug messages
void printDebug(const String& message) {
  Serial.println("[DEBUG] " + message);
}

// Reads from modem serial until a timeout or a specific string is found
String readModemResponse(unsigned long timeout, const char* term = nullptr) {
  String response = "";
  unsigned long start = millis();
  while (millis() - start < timeout) {
    if (modemSerial.available()) {
      char c = modemSerial.read();
      response += c;
      if (term && response.endsWith(term)) {
        break;
      }
    }
  }
  return response;
}

// Sends an AT command and waits for "OK" or "ERROR"
bool sendAtCommand(const String& cmd, unsigned long timeout = 1000) {
    Serial.println("[AT SEND] " + cmd);
    modemSerial.println(cmd);
    String response = readModemResponse(timeout, "OK");
    if (response.indexOf("OK") != -1) {
        return true;
    }
    printDebug("Command failed. Response: " + response);
    return false;
}


// --- Core Logic ---

// Configure SSL for HTTPS - This should only be called AFTER GPRS is connected.
bool configureSSL() {
  Serial.println("🔧 Configuring SSL...");
  if (!sendAtCommand("AT+CSSLCFG=\"sslversion\",1,3")) return false; // Set TLS 1.2
  if (!sendAtCommand("AT+CSSLCFG=\"authmode\",1,0")) return false;   // No server cert validation
  if (!sendAtCommand("AT+HTTPSSL=1")) return false; // Enable SSL for HTTP commands
  
  Serial.println("✅ SSL Configured.");
  return true;
}

// Wait for the modem to be fully initialized and ready.
bool waitForModemReady() {
  Serial.println("⏳ Waiting for modem to be ready...");
  for (int i = 0; i < 30; i++) { // Wait up to 15 seconds
    if (modem.testAT(1000)) {
       String simStatus = modem.getSimStatus();
       if (simStatus == "READY") {
          int registrationStatus = modem.getRegistrationStatus();
          if (registrationStatus == 1 || registrationStatus == 5) {
             Serial.println("✅ Modem is ready and registered.");
             return true;
          }
           Serial.print(".");
       }
    }
    delay(500);
  }
  return false;
}


// --- File Upload Logic ---

bool sendChunk(File& file, const char* filename, size_t offset, size_t totalSize) {
    const size_t CHUNK_BUFFER_SIZE = 4096;
    uint8_t chunkBuffer[CHUNK_BUFFER_SIZE];

    size_t chunkSize = file.read(chunkBuffer, CHUNK_BUFFER_SIZE);
    if (chunkSize == 0) {
        printDebug("Finished reading file.");
        return true; 
    }

    String headers = "x-filename: " + String(filename) + "\r\n";
    headers += "x-chunk-offset: " + String(offset) + "\r\n";
    headers += "x-chunk-size: " + String(chunkSize) + "\r\n";
    headers += "x-total-size: " + String(totalSize) + "\r\n";
    headers += "Content-Type: application/octet-stream";

    String atCommand = "AT+CHTTPSPOST=\"" + String(resource) + "\"," + String(headers.length()) + "," + String(chunkSize) + ",10000";

    for (int retry = 0; retry < 3; retry++) {
        Serial.printf("  Attempt %d/3 to send chunk at offset %u...\n", retry + 1, offset);
        
        // Open the HTTPS session each time
        if (!sendAtCommand("AT+CHTTPSOPSE=\"" + String(server) + "\"," + String(port))) {
            printDebug("Failed to open HTTPS session.");
            sendAtCommand("AT+CHTTPSCLSE"); // Try to close it just in case
            continue;
        }

        Serial.println("[AT SEND] " + atCommand);
        modemSerial.println(atCommand);

        String promptResponse = readModemResponse(5000, ">");
        if (promptResponse.indexOf('>') == -1) {
            printDebug("Modem did not respond to POST command. Received: " + promptResponse);
            sendAtCommand("AT+CHTTPSCLSE");
            continue;
        }

        // Send headers
        modemSerial.print(headers);
        modemSerial.print("\r\n\r\n");

        // Send binary data
        modemSerial.write(chunkBuffer, chunkSize);
        
        String postResponse = readModemResponse(15000, "OK");
        sendAtCommand("AT+CHTTPSCLSE"); // Always close the session

        if (postResponse.indexOf("+CHTTPS: POST,20") != -1) { // Check for 200 or 201
            Serial.printf("  ✅ Chunk at offset %u sent successfully.\n", offset);
            return true;
        }
        
        printDebug("Chunk upload failed. Response: " + postResponse);
    }

    Serial.printf("❌ Failed to upload chunk at offset %u after 3 retries.\n", offset);
    return false;
}

void uploadFile(const char* filename) {
    Serial.printf("📤 Preparing to upload %s\n", filename);

    if (!SD.exists(filename)) {
        Serial.printf("❌ File %s does not exist on SD card.\n", filename);
        return;
    }

    File file = SD.open(filename, FILE_READ);
    if (!file) {
        Serial.println("❌ Failed to open file for reading.");
        return;
    }

    size_t totalSize = file.size();
    Serial.printf("  File size: %u bytes\n", totalSize);

    size_t offset = 0;
    while (offset < totalSize) {
        if (!sendChunk(file, filename, offset, totalSize)) {
            Serial.println("❌ Aborting file upload due to chunk failure.");
            file.close();
            return;
        }
        offset = file.position();
    }

    Serial.println("✅ File upload completed successfully.");
    file.close();
}


// --- Arduino Setup & Loop ---

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("🔌 Booting...");

  // Initialize SD card
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("❌ SD card initialization failed!");
    while (1);
  }
  Serial.println("✅ SD card ready.");

  // Initialize modem
  Serial.println("Initializing modem...");
  modemSerial.begin(MODEM_BAUD, SERIAL_8N1, MODEM_RX, MODEM_TX);
  
  if (!waitForModemReady()) {
      Serial.println("❌ Modem failed to initialize. Halting.");
      while(1);
  }
  
  // Connect to GPRS
  Serial.println("📡 Connecting to network...");
  if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
      Serial.println("❌ GPRS connection failed.");
      while(1);
  }
  Serial.println("✅ GPRS connected.");
  Serial.print("Local IP: ");
  Serial.println(modem.getLocalIP());

  // Configure SSL for HTTPS now that GPRS is up
  if (!configureSSL()) {
      Serial.println("❌ Failed to configure SSL. Halting.");
      while(1);
  }
  
  // Start the upload process
  if (!sendAtCommand("AT+CHTTPSSTART")) {
     Serial.println("❌ Failed to start HTTPS service. Halting.");
     while(1);
  }
  
  uploadFile(filename);

  // Stop HTTPS service when done
  sendAtCommand("AT+CHTTPSSTOP");
  Serial.println("🏁 Process finished.");
}

void loop() {
  // All logic is in setup for this one-shot example.
  delay(10000);
}
