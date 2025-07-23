
// Define the serial port for the modem
#define SerialAT Serial1

// Define the pins for the modem
#define UART_TX 17
#define UART_RX 16
#define SD_CS   5

// Define the name of the file to upload
#define FILE_TO_UPLOAD "/sigma2.wav"

// Your GPRS credentials
const char apn[] = "internet"; // APN
const char gprsUser[] = "";    // GPRS User
const char gprsPass[] = "";    // GPRS Password

// Server details
const char server[] = "6000-firebase-studio-1753223410587.cluster-73qgvk7hjjadkrjeyexca5ivva.cloudworkstations.dev";
const int port = 443;

#define TINY_GSM_MODEM_SIM7600
#include <TinyGsmClient.h>
#include <SPI.h>
#include <SD.h>
#include <SSLClient.h>

TinyGsm modem(SerialAT);
TinyGsmClient baseClient(modem);
SSLClient client(baseClient);


void setup() {
  Serial.begin(115200);
  delay(10);

  SerialAT.begin(115200, SERIAL_8N1, UART_RX, UART_TX);
  delay(6000);

  Serial.println(F("? Initializing modem..."));
  if (!modem.init()) {
    Serial.println(F("? Failed to init modem."));
    return;
  }
  
  // Set modem to 4G/LTE mode
  modem.setNetworkMode(38);
  modem.setPhoneFunctionality(1);

  Serial.print(F("? Waiting for network..."));
  if (!modem.waitForNetwork()) {
    Serial.println(F(" fail"));
    delay(10000);
    return;
  }
  Serial.println(F(" success"));

  if (modem.isNetworkConnected()) {
    Serial.println(F("? Network connected"));
  }

  Serial.print(F("? Connecting to "));
  Serial.print(apn);
  if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
    Serial.println(F(" fail"));
    delay(10000);
    return;
  }
  Serial.println(F(" success"));

  if (modem.isGprsConnected()) {
    Serial.println(F("? GPRS connected"));
  }
  
  // Allow anonymous connection
  client.setInsecure();

  Serial.println(F("? Initializing SD card..."));
  if (!SD.begin(SD_CS)) {
    Serial.println(F("? SD Card initialization failed!"));
    return;
  }
  Serial.println(F("? SD card initialized."));

  uploadFile(FILE_TO_UPLOAD);
}

void loop() {
  // Keep the modem alive
  modem.maintain();
  delay(1000);
}

void uploadFile(const char* filename) {
  File file = SD.open(filename);
  if (!file) {
    Serial.println(F("? Failed to open file for reading"));
    return;
  }

  size_t fileSize = file.size();
  Serial.print(F("? Uploading file: "));
  Serial.print(filename);
  Serial.print(F(" ("));
  Serial.print(fileSize);
  Serial.println(F(" bytes)"));

  const size_t chunkSize = 4096; // 4KB chunk size
  size_t offset = 0;
  
  while (offset < fileSize) {
    Serial.print(F("? Connecting to "));
    Serial.println(server);
    if (!client.connect(server, port)) {
      Serial.println(F("? Connection failed!"));
      file.close();
      return;
    }
    Serial.println(F("? Connected."));

    size_t bytesToSend = min(chunkSize, fileSize - offset);
    
    // Manually construct the HTTP POST request
    client.print(F("POST /api/upload HTTP/1.1\r\n"));
    client.print(F("Host: "));
    client.print(server);
    client.print(F("\r\n"));
    client.print(F("User-Agent: Arduino/1.0\r\n"));
    client.print(F("Connection: close\r\n"));
    client.print(F("x-filename: "));
    client.print(filename + 1); // Remove leading '/'
    client.print(F("\r\n"));
    client.print(F("x-chunk-offset: "));
    client.print(offset);
    client.print(F("\r\n"));
    client.print(F("x-chunk-size: "));
    client.print(bytesToSend);
    client.print(F("\r\n"));
    client.print(F("x-total-size: "));
    client.print(fileSize);
    client.print(F("\r\n"));
    client.print(F("Content-Type: application/octet-stream\r\n"));
    client.print(F("Content-Length: "));
    client.print(bytesToSend);
    client.print(F("\r\n\r\n"));
    
    // Read chunk from file and send
    uint8_t buffer[128];
    size_t totalBytesSent = 0;
    while(totalBytesSent < bytesToSend) {
        size_t bytesToRead = min(sizeof(buffer), bytesToSend - totalBytesSent);
        size_t bytesRead = file.read(buffer, bytesToRead);
        if(bytesRead == 0) break;
        client.write(buffer, bytesRead);
        totalBytesSent += bytesRead;
    }

    Serial.print(F("? Sent chunk at offset "));
    Serial.print(offset);
    Serial.print(F(" with size "));
    Serial.println(bytesToSend);
    
    // Wait for response
    unsigned long timeout = millis();
    while (client.connected() && millis() - timeout < 10000L) {
      if (client.available()) {
        String line = client.readStringUntil('\n');
        Serial.println(line);
        if (line.startsWith("HTTP/1.1 200")) {
            Serial.println(F("? Chunk uploaded successfully."));
        }
      }
    }
    
    client.stop();
    Serial.println("? Connection closed.");

    offset += bytesToSend;
  }
  
  file.close();
  Serial.println(F("? File upload finished."));
}

size_t min(size_t a, size_t b) {
    return (a < b) ? a : b;
}
