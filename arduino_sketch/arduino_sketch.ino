
#define TINY_GSM_MODEM_SIM7600
#define SerialMon Serial
#define SerialAT Serial1

#include <Arduino.h>
#include <TinyGsmClient.h>
#include <SSLClient.h>
#include <SD.h>
#include <SPI.h>

// Pin definitions
const int TX_PIN = 27;
const int RX_PIN = 26;
const int SD_CS_PIN = 5;

// Modem and network configuration
const char apn[] = "internet"; // APN
const char gprsUser[] = "";    // GPRS User
const char gprsPass[] = "";    // GPRS Password

// Server configuration
const char server[] = "6000-firebase-studio-1753223410587.cluster-73qgvk7hjjadkrjeyexca5ivva.cloudworkstations.dev";
const char resource[] = "/api/upload";
const int port = 443;

// TinyGSM modem and client
HardwareSerial SerialAT(1);
TinyGsm modem(SerialAT);
TinyGsmClient baseClient(modem);
SSLClient client(baseClient);

void setup() {
  SerialMon.begin(115200);
  delay(10);

  SerialMon.println("Initializing modem...");
  SerialAT.begin(115200, SERIAL_8N1, RX_PIN, TX_PIN);
  modem.init();
  String modemInfo = modem.getModemInfo();
  SerialMon.print("Modem Info: ");
  SerialMon.println(modemInfo);

  // Set to LTE-only mode
  SerialMon.println("Setting modem to LTE-only mode...");
  if (!modem.setNetworkMode(38)) {
    SerialMon.println("Failed to set network mode");
    while (true);
  }

  // Set to full functionality
  SerialMon.println("Setting modem to full functionality...");
  if (!modem.setFunctionality(1)) {
      SerialMon.println("Failed to set full functionality");
      while(true);
  }

  SerialMon.println("Initializing SD card...");
  if (!SD.begin(SD_CS_PIN)) {
    SerialMon.println("SD Card initialization failed!");
    while (true);
  }
  SerialMon.println("SD Card initialized.");

  SerialMon.println("Waiting for network...");
  if (!modem.waitForNetwork()) {
    SerialMon.println("Failed to connect to network");
    while (true);
  }
  SerialMon.println("Network connected.");

  SerialMon.println("Connecting to GPRS...");
  if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
    SerialMon.println("Failed to connect to GPRS");
    while (true);
  }
  SerialMon.println("GPRS connected.");

  String ip = modem.getLocalIP();
  SerialMon.print("IP Address: ");
  SerialMon.println(ip);

  // Start the upload process
  uploadFile("/sigma2.wav");
}

void loop() {
  // Everything is done in setup
}

void uploadFile(const char *filePath) {
  File file = SD.open(filePath);
  if (!file) {
    SerialMon.println("Failed to open file for reading");
    return;
  }
  size_t fileSize = file.size();
  SerialMon.print("Uploading file: ");
  SerialMon.print(filePath);
  SerialMon.print(" (");
  SerialMon.print(fileSize);
  SerialMon.println(" bytes)");

  SerialMon.print("Connecting to ");
  SerialMon.print(server);
  SerialMon.print(":");
  SerialMon.println(port);

  // Attempt to connect
  if (!client.connect(server, port)) {
    SerialMon.println("Connection failed");
    file.close();
    return;
  }
  SerialMon.println("Connected to server.");

  // Make a HTTP request
  String originalFilename = String(filePath);
  if (originalFilename.lastIndexOf('/') >= 0) {
      originalFilename = originalFilename.substring(originalFilename.lastIndexOf('/') + 1);
  }
  
  client.print(String("POST ") + resource + " HTTP/1.1\r\n");
  client.print(String("Host: ") + server + "\r\n");
  client.print("Connection: close\r\n");
  client.print("X-Filename: " + originalFilename + "\r\n");
  client.print("X-Total-Size: " + String(fileSize) + "\r\n");
  client.print("Content-Type: application/octet-stream\r\n");
  client.print(String("Content-Length: ") + fileSize + "\r\n");
  client.print("\r\n");

  // Send file content
  const size_t bufferSize = 1024;
  byte buffer[bufferSize];
  size_t bytesSent = 0;
  while (file.available()) {
    size_t bytesRead = file.read(buffer, bufferSize);
    if (bytesRead > 0) {
      client.write(buffer, bytesRead);
      bytesSent += bytesRead;
      SerialMon.print("Sent ");
      SerialMon.print(bytesSent);
      SerialMon.print("/");
      SerialMon.print(fileSize);
      SerialMon.println(" bytes");
    }
  }

  file.close();
  SerialMon.println("File sent.");

  // Wait for response
  unsigned long timeout = millis();
  while (client.available() == 0) {
    if (millis() - timeout > 30000) {
      SerialMon.println(">>> Client Timeout !");
      client.stop();
      return;
    }
  }

  // Read response
  SerialMon.println("Reading response:");
  while (client.available()) {
    char c = client.read();
    SerialMon.write(c);
  }

  client.stop();
  SerialMon.println("Connection closed.");
}
