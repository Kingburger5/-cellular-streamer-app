#define TINY_GSM_MODEM_SIM7600
#include <HardwareSerial.h>
#include <TinyGsmClient.h>
#include <SD.h>
#include <SPI.h>

// Your GPRS credentials, if any
const char apn[]  = "vodafone";
const char gprsUser[] = "";
const char gprsPass[] = "";

// Server details
const char server[] = "cellular-data-streamer.web.app";
const char resource[] = "/api/upload";
const int  port = 80; // Using HTTP for simplicity with TinyGSM

#define MODEM_TX 17
#define MODEM_RX 16
#define MODEM_BAUD 115200
#define SD_CS 5
#define CHUNK_SIZE 1024 * 5 // 5KB chunk size for efficiency

HardwareSerial simSerial(1);
TinyGsm modem(simSerial);
TinyGsmClient client(modem);

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("🔌 Booting...");

  if (!SD.begin(SD_CS)) {
    Serial.println("❌ SD card failed.");
    while (true);
  }
  Serial.println("✅ SD card ready.");

  simSerial.begin(MODEM_BAUD, SERIAL_8N1, MODEM_RX, MODEM_TX);
  delay(3000);
  
  Serial.println("Initializing modem...");
  modem.restart();
  
  String modemInfo = modem.getModemInfo();
  Serial.print("Modem Info: ");
  Serial.println(modemInfo);

  Serial.println("📡 Connecting to network...");
  if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
      Serial.println("❌ GPRS connect failed.");
      while(true);
  }

  bool res = modem.isGprsConnected();
  Serial.print("GPRS status: ");
  Serial.println(res ? "connected" : "not connected");

  String ccid = modem.getSimCCID();
  Serial.print("SIM CCID: ");
  Serial.println(ccid);

  String imei = modem.getIMEI();
  Serial.print("IMEI: ");
  Serial.println(imei);

  String cop = modem.getOperator();
  Serial.print("Operator: ");
  Serial.println(cop);

  IPAddress local = modem.localIP();
  Serial.print("Local IP: ");
  Serial.println(local);
  
  Serial.println("\n✅ Modem and network ready.");

  // Upload your file here
  uploadFile("/sigma2.wav");
}

void loop() {
  // Kept empty.
}

void uploadFile(const char* originalFilename) {
  File file = SD.open(originalFilename, FILE_READ);
  if (!file) {
    Serial.printf("❌ Failed to open file: %s\n", originalFilename);
    return;
  }

  size_t fileSize = file.size();
  String fileIdentifier = String(originalFilename) + "-" + String(fileSize) + "-" + String(file.getLastWrite());
  int totalChunks = (fileSize + CHUNK_SIZE - 1) / CHUNK_SIZE;

  Serial.printf("📤 Uploading %s (%u bytes) in %d chunks.\n", originalFilename, fileSize, totalChunks);

  for (int chunkIndex = 0; chunkIndex < totalChunks; chunkIndex++) {
    size_t offset = chunkIndex * CHUNK_SIZE;
    size_t chunkSize = min((size_t)CHUNK_SIZE, fileSize - offset);
    
    uint8_t buffer[chunkSize];
    file.read(buffer, chunkSize);

    String boundary = "----WebKitFormBoundary7MA4YWxkTrZu0gW";
    
    // Construct the multipart form data body
    String data_start = "--" + boundary + "\r\n";
    data_start += "Content-Disposition: form-data; name=\"chunk\"; filename=\"" + String(originalFilename) + "\"\r\n";
    data_start += "Content-Type: application/octet-stream\r\n\r\n";
    
    String data_end = "\r\n";
    data_end += "--" + boundary + "\r\n";
    data_end += "Content-Disposition: form-data; name=\"fileIdentifier\"\r\n\r\n" + fileIdentifier + "\r\n";
    data_end += "--" + boundary + "\r\n";
    data_end += "Content-Disposition: form-data; name=\"chunkIndex\"\r\n\r\n" + String(chunkIndex) + "\r\n";
    data_end += "--" + boundary + "\r\n";
    data_end += "Content-Disposition: form-data; name=\"totalChunks\"\r\n\r\n" + String(totalChunks) + "\r\n";
    data_end += "--" + boundary + "\r\n";
    data_end += "Content-Disposition: form-data; name=\"originalFilename\"\r\n\r\n" + String(originalFilename) + "\r\n";
    data_end += "--" + boundary + "--\r\n";

    long contentLength = data_start.length() + chunkSize + data_end.length();

    Serial.printf("Connecting to %s...", server);
    if (!client.connect(server, port)) {
      Serial.println(" ❌ connection failed");
      delay(5000);
      continue;
    }
    Serial.println(" ✅");

    // Send HTTP POST request
    client.println(String("POST ") + resource + " HTTP/1.1");
    client.println(String("Host: ") + server);
    client.println("Connection: keep-alive");
    client.println(String("Content-Type: multipart/form-data; boundary=") + boundary);
    client.println(String("Content-Length: ") + contentLength);
    client.println();

    // Send multipart data
    client.print(data_start);
    client.write(buffer, chunkSize);
    client.print(data_end);
    client.flush();

    Serial.printf("📤 Uploading chunk %d/%d (%d bytes)\n", chunkIndex + 1, totalChunks, chunkSize);

    // Read the response
    unsigned long timeout = millis();
    bool success = false;
    while (client.connected() && millis() - timeout < 10000L) {
      if (client.available()) {
        String line = client.readStringUntil('\n');
        line.trim();
        Serial.println("<< " + line);
        if (line.startsWith("HTTP/1.1 200") || line.startsWith("HTTP/1.1 201")) {
           success = true;
        }
      }
    }

    if (success) {
      Serial.println("✅ Chunk uploaded successfully.");
    } else {
      Serial.println("❌ Chunk upload failed.");
      client.stop();
      file.close();
      return; // Stop on first failure
    }
    
    client.stop();
    delay(500); // Small delay between chunks
  }

  file.close();
  Serial.println("\n✅ File upload complete.");
}
