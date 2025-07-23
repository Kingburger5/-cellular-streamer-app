
// Define the serial connections
#define SerialAT Serial1
#define TINY_GSM_MODEM_SIM7600

// Define the pins for the modem
#define MODEM_RST        2
#define MODEM_PWRKEY     4
#define MODEM_RX         16
#define MODEM_TX         17
#define SD_CS_PIN        5

// Define connection settings
const char apn[]      = "internet";
const char gprsUser[] = "";
const char gprsPass[] = "";
const char server[]   = "6000-firebase-studio-1753223410587.cluster-73qgvk7hjjadkrjeyexca5ivva.cloudworkstations.dev";
const char resource[] = "/api/upload";
const int  port       = 443;

// Include necessary libraries
#include <SPI.h>
#include <SD.h>
#include <TinyGsmClient.h>

// Uncomment this to see more debug information
#define DUMP_AT_COMMANDS

#ifdef DUMP_AT_COMMANDS
  #include <StreamDebugger.h>
  StreamDebugger debugger(SerialAT, Serial);
  TinyGsm modem(debugger);
#else
  TinyGsm modem(SerialAT);
#endif

TinyGsmClientSecure client(modem);

const char* FILENAME_TO_UPLOAD = "/sigma2.wav";
const int CHUNK_SIZE = 4096; // 4KB chunks

void setup() {
  // Start the primary serial port for debugging
  Serial.begin(115200);
  delay(10);

  // Set modem pins
  pinMode(MODEM_PWRKEY, OUTPUT);
  digitalWrite(MODEM_PWRKEY, LOW);
  delay(100);
  digitalWrite(MODEM_PWRKEY, HIGH);
  delay(1000);
  digitalWrite(MODEM_PWRKEY, LOW);

  // Start the serial connection to the modem
  SerialAT.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);
  delay(3000);

  Serial.println(F("? Initializing modem..."));
  if (!modem.restart()) {
    Serial.println(F("! Failed to restart modem, halting."));
    while (true);
  }

  // Initialize the SD card
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println(F("! SD card initialization failed!"));
    return;
  }
  Serial.println(F("? SD card ready."));
  
  // Connect to GPRS
  Serial.print(F("? Connecting to GPRS: "));
  Serial.println(apn);
  if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
    Serial.println(F("! GPRS connection failed."));
    return;
  }
  
  Serial.print(F("? GPRS Connected. IP: "));
  Serial.println(modem.getLocalIP());

  // Start the upload process
  uploadFile(FILENAME_TO_UPLOAD);
}

void loop() {
  // The main process runs once in setup()
}

void uploadFile(const char* filename) {
  File file = SD.open(filename, FILE_READ);
  if (!file) {
    Serial.print(F("! Failed to open file for reading: "));
    Serial.println(filename);
    return;
  }

  size_t fileSize = file.size();
  Serial.print(F("? Preparing to upload "));
  Serial.print(filename);
  Serial.print(F(" ("));
  Serial.print(fileSize);
  Serial.println(F(" bytes)"));

  size_t bytesSent = 0;
  while (bytesSent < fileSize) {
    size_t chunkSize = (fileSize - bytesSent < CHUNK_SIZE) ? (fileSize - bytesSent) : CHUNK_SIZE;

    Serial.print(F("? Uploading chunk at offset "));
    Serial.print(bytesSent);
    Serial.print(F(" ("));
    Serial.print(chunkSize);
    Serial.println(F(" bytes)..."));

    // Connect to server
    if (!client.connect(server, port)) {
      Serial.println(F("! Connection to server failed. Retrying..."));
      delay(5000);
      continue;
    }

    // Send HTTP POST headers
    client.print(F("POST "));
    client.print(resource);
    client.println(F(" HTTP/1.1"));
    client.print(F("Host: "));
    client.println(server);
    client.println(F("Connection: close"));
    client.print(F("Content-Length: "));
    client.println(chunkSize);
    client.print(F("Content-Type: application/octet-stream\r\n"));
    client.print(F("X-Filename: "));
    client.println(filename + 1); // Remove leading '/'
    client.print(F("X-Chunk-Offset: "));
    client.println(bytesSent);
    client.print(F("X-Chunk-Size: "));
    client.println(chunkSize);
    client.print(F("X-Total-Size: "));
    client.println(fileSize);
    client.println(); // End of headers

    // Send the chunk data
    uint8_t buffer[256];
    size_t toRead = chunkSize;
    while(toRead > 0) {
      size_t willRead = (toRead < sizeof(buffer)) ? toRead : sizeof(buffer);
      size_t bytesRead = file.read(buffer, willRead);
      if (bytesRead > 0) {
        client.write(buffer, bytesRead);
      }
      toRead -= bytesRead;
    }
    
    // Wait for the server's response
    unsigned long timeout = millis();
    while (client.connected() && millis() - timeout < 10000L) {
      if (client.available()) {
        String line = client.readStringUntil('\n');
        Serial.print(F("[SERVER] "));
        Serial.println(line);
        if (line.startsWith("HTTP/1.1 200 OK")) {
          bytesSent += chunkSize;
        }
      }
    }
    
    // Close connection
    if (client.connected()) {
      client.stop();
    }
    Serial.println(F("? Connection closed."));
  }

  Serial.println(F("? File upload complete."));
  file.close();
}
