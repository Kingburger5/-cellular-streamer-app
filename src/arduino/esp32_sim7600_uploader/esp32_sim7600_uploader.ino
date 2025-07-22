
// A simple example for a HTTP POST request with a file.
// created by Firebase AI on 07 May 2024
//
// This sketch has been written to work with the ESP32 and the SIM7600 module.
//
// You need to install the following libraries:
// - TinyGSM: https://github.com/vshymanskyy/TinyGSM
// - ArduinoHttpClient: https://github.com/arduino-libraries/ArduinoHttpClient

// Define your server details
const char server[] = "your-app-url.firebaseapp.com"; // Replace with your server's URL
const char resource[] = "/api/upload";
const int port = 80;

// Set your APN credentials (check with your SIM provider)
const char apn[] = "your_apn";
const char gprsUser[] = "";
const char gprsPass[] = "";

// SIM7600 serial communication
#define SerialMon Serial
#define SerialAT Serial1

#define TINY_GSM_MODEM_SIM7600
#define TINY_GSM_RX_BUFFER 1024

#include <TinyGsmClient.h>
#include <ArduinoHttpClient.h>
#include <SD.h>
#include <SPI.h>

#ifdef DUMP_AT_COMMANDS
#include <StreamDebugger.h>
StreamDebugger debugger(SerialAT, SerialMon);
TinyGsm modem(debugger);
#else
TinyGsm modem(SerialAT);
#endif

TinyGsmClient client(modem);
HttpClient http(client, server, port);

// SD Card Configuration
const int SD_CS_PIN = 5; // Change this to your SD card CS pin

void setup() {
  SerialMon.begin(115200);
  delay(10);

  SerialMon.println("Initializing modem...");
  SerialAT.begin(115200); // Adjust baud rate if needed
  delay(6000);

  if (!modem.init()) {
    SerialMon.println("Failed to init modem. Restarting...");
    ESP.restart();
  }

  SerialMon.print("Connecting to APN: ");
  SerialMon.print(apn);
  if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
    SerialMon.println("... failed");
    delay(5000);
    return;
  }
  SerialMon.println("... success");

  SerialMon.println("Initializing SD card...");
  if (!SD.begin(SD_CS_PIN)) {
    SerialMon.println("SD Card initialization failed!");
    return;
  }
  SerialMon.println("SD Card initialized.");
}

void loop() {
  SerialMon.println("Enter the filename to upload (e.g., data.txt):");
  while (!SerialMon.available()) {
    delay(100);
  }
  String filename = SerialMon.readStringUntil('\n');
  filename.trim();

  if (filename.length() > 0) {
    uploadFile(filename);
  }
}

void uploadFile(String filename) {
  File file = SD.open("/" + filename, FILE_READ);
  if (!file) {
    SerialMon.println("Failed to open file for reading");
    return;
  }

  const size_t chunkSize = 1024 * 10; // 10KB chunk size, adjust as needed
  size_t fileSize = file.size();
  int totalChunks = (fileSize + chunkSize - 1) / chunkSize;

  // Create a unique identifier for the file
  String fileIdentifier = filename + "-" + String(fileSize) + "-" + String(millis());

  SerialMon.print("Uploading file: ");
  SerialMon.print(filename);
  SerialMon.print(", Size: ");
  SerialMon.print(fileSize);
  SerialMon.print(" bytes, Chunks: ");
  SerialMon.println(totalChunks);

  for (int i = 0; i < totalChunks; i++) {
    byte buffer[chunkSize];
    size_t bytesRead = file.read(buffer, sizeof(buffer));
    
    if (bytesRead > 0) {
      SerialMon.print("Uploading chunk ");
      SerialMon.print(i + 1);
      SerialMon.print("/");
      SerialMon.print(totalChunks);
      SerialMon.print("... ");

      if (sendChunk(buffer, bytesRead, fileIdentifier, filename, i, totalChunks)) {
        SerialMon.println("Success");
      } else {
        SerialMon.println("Failed");
        file.close();
        return; // Stop on first failure
      }
    }
  }

  file.close();
  SerialMon.println("File upload complete.");
}

bool sendChunk(const byte* buffer, size_t size, String fileIdentifier, String originalFilename, int chunkIndex, int totalChunks) {
  String boundary = "----WebKitFormBoundary" + String(millis());
  String contentType = "multipart/form-data; boundary=" + boundary;

  // Build the request body
  String body_start = "--" + boundary + "\r\n";
  body_start += "Content-Disposition: form-data; name=\"chunk\"; filename=\"" + originalFilename + "\"\r\n";
  body_start += "Content-Type: application/octet-stream\r\n\r\n";

  String body_end = "\r\n--" + boundary + "\r\n";
  body_end += "Content-Disposition: form-data; name=\"fileIdentifier\"\r\n\r\n";
  body_end += fileIdentifier + "\r\n";
  body_end += "--" + boundary + "\r\n";
  body_end += "Content-Disposition: form-data; name=\"chunkIndex\"\r\n\r\n";
  body_end += String(chunkIndex) + "\r\n";
  body_end += "--" + boundary + "\r\n";
  body_end += "Content-Disposition: form-data; name=\"totalChunks\"\r\n\r\n";
  body_end += String(totalChunks) + "\r\n";
  body_end += "--" + boundary + "\r\n";
  body_end += "Content-Disposition: form-data; name=\"originalFilename\"\r\n\r\n";
  body_end += originalFilename + "\r\n";
  body_end += "--" + boundary + "--\r\n";

  long contentLength = body_start.length() + size + body_end.length();

  if (!http.connect(server, port)) {
    SerialMon.println("... failed to connect");
    return false;
  }

  // Send headers
  http.print(String("POST ") + resource + " HTTP/1.1\r\n");
  http.print(String("Host: ") + server + "\r\n");
  http.print("Content-Type: " + contentType + "\r\n");
  http.print("Content-Length: " + String(contentLength) + "\r\n");
  http.print("Connection: close\r\n");
  http.print("\r\n");

  // Send body
  http.print(body_start);
  client.write(buffer, size);
  http.print(body_end);
  
  // Read response
  int statusCode = http.responseStatusCode();
  String response = http.responseBody();

  http.stop();

  if (statusCode == 200 || statusCode == 201) {
    return true;
  } else {
    SerialMon.print("Error: ");
    SerialMon.println(statusCode);
    SerialMon.println(response);
    return false;
  }
}

