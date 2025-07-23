
// Define the modem type for the TinyGSM library.
#define TINY_GSM_MODEM_SIM7600

// Define the serial ports for communication.
#define SerialMon Serial
#define SerialAT Serial1

// Include necessary libraries.
#include <SPI.h>
#include <SD.h>
#include <TinyGsmClient.h>
#include <StreamDebugger.h>

// Modem and GPRS settings.
const char apn[]      = "internet";
const char gprsUser[] = "";
const char gprsPass[] = "";

// Server settings.
const char server[] = "6000-firebase-studio-1753223410587.cluster-73qgvk7hjjadkrjeyexca5ivva.cloudworkstations.dev";
const int  port = 443;

// Pin definitions for UART and SD card.
#define UART_BAUD           115200
#define UART_RX_PIN         26
#define UART_TX_PIN         27

#define SD_MISO             13
#define SD_MOSI             12
#define SD_SCLK             14
#define SD_CS               5

#define CHUNK_SIZE 4096

// The TinyGSM modem object
TinyGsm modem(SerialAT);

// The secure GSM client for HTTPS
TinyGsmClientSecure client(modem);

// StreamDebugger for printing AT commands to the serial monitor.
StreamDebugger debugger(SerialAT, SerialMon);

void setup() {
  // Start the serial monitor.
  SerialMon.begin(115200);
  delay(10);

  SerialMon.println(F("? Booting..."));

  // Initialize the SD card.
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);
  SPI.begin(SD_SCLK, SD_MISO, SD_MOSI, SD_CS);
  if (!SD.begin(SD_CS)) {
    SerialMon.println(F("? SD card initialization failed!"));
    while (1);
  }
  SerialMon.println(F("? SD card ready."));
  
  // Initialize the modem.
  SerialMon.println(F("? Initializing modem..."));
  SerialAT.begin(UART_BAUD, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);
  delay(6000); // Wait for the modem to power on.
  
  // Set modem to full functionality
  modem.sendAT(F("+CFUN=1"));
  modem.waitResponse();
  
  // Set modem to 4G/LTE mode
  modem.sendAT(F("+CNMP=38"));
  modem.waitResponse();

  // Restart the modem to apply settings
  if (!modem.restart()) {
    SerialMon.println(F("? Failed to restart modem."));
  }

  String modemInfo = modem.getModemInfo();
  SerialMon.print(F("? Modem Info: "));
  SerialMon.println(modemInfo);

  // Unlock the SIM card if necessary.
  // if (modem.getSimStatus() != 3) {
  //   modem.simUnlock("1234");
  // }

  uploadFile("/sigma2.wav");
}

void loop() {
  // The main logic is in setup() for this single-task sketch.
  delay(10000);
}

void uploadFile(const char* filename) {
  // Connect to the GPRS network.
  SerialMon.println(F("? Connecting to GPRS..."));
  if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
    SerialMon.println(F("? GPRS connection failed."));
    return;
  }
  SerialMon.println(F("? GPRS connected."));
  
  String ip = modem.getLocalIP();
  SerialMon.print(F("? IP Address: "));
  SerialMon.println(ip);

  // Open the file from the SD card.
  File file = SD.open(filename, FILE_READ);
  if (!file) {
    SerialMon.println(F("? Failed to open file for reading."));
    return;
  }
  size_t fileSize = file.size();
  SerialMon.print(F("? Preparing to upload "));
  SerialMon.print(filename);
  SerialMon.print(F(" ("));
  SerialMon.print(fileSize);
  SerialMon.println(F(" bytes)"));

  size_t bytesSent = 0;
  while (bytesSent < fileSize) {
    SerialMon.print("? Connecting to ");
    SerialMon.print(server);
    SerialMon.println("...");

    if (!client.connect(server, port)) {
      SerialMon.println(F("? Connection failed."));
      file.close();
      return;
    }
    SerialMon.println(F("? Connected."));
    
    // Determine the size of the current chunk.
    size_t chunkSize = CHUNK_SIZE;
    if (bytesSent + chunkSize > fileSize) {
      chunkSize = fileSize - bytesSent;
    }

    // Build the HTTP request headers.
    String headers = "POST /api/upload HTTP/1.1\r\n";
    headers += "Host: " + String(server) + "\r\n";
    headers += "X-Filename: " + String(filename).substring(1) + "\r\n";
    headers += "X-Chunk-Offset: " + String(bytesSent) + "\r\n";
    headers += "X-Chunk-Size: " + String(chunkSize) + "\r\n";
    headers += "X-Total-Size: " + String(fileSize) + "\r\n";
    headers += "Content-Type: application/octet-stream\r\n";
    headers += "Content-Length: " + String(chunkSize) + "\r\n";
    headers += "Connection: close\r\n\r\n";
    
    // Send the headers.
    client.print(headers);
    SerialMon.println(F("? Sent headers."));
    
    // Read the chunk from the file into a buffer.
    uint8_t buffer[chunkSize];
    file.seek(bytesSent);
    size_t bytesRead = file.read(buffer, chunkSize);

    // Send the chunk data.
    client.write(buffer, bytesRead);
    SerialMon.print(F("? Sent chunk of size: "));
    SerialMon.println(bytesRead);

    // Wait for the server's response.
    unsigned long timeout = millis();
    while (client.connected() && millis() - timeout < 10000L) {
      if (client.available()) {
        String line = client.readStringUntil('\n');
        SerialMon.print(F("<-- "));
        SerialMon.println(line);
        if (line.startsWith("HTTP/1.1 200 OK") || line.startsWith("HTTP/1.1 200")) {
           SerialMon.println(F("? Chunk uploaded successfully."));
        }
      }
    }
    
    // Update the number of bytes sent.
    bytesSent += bytesRead;
    
    // Stop the client connection.
    client.stop();
    SerialMon.println(F("? Connection closed."));
    
    SerialMon.print(F("? Upload progress: "));
    SerialMon.print(bytesSent);
    SerialMon.print(F(" / "));
    SerialMon.print(fileSize);
    SerialMon.println(F(" bytes"));
  }

  // Close the file.
  file.close();
  SerialMon.println(F("? File upload complete."));
}
