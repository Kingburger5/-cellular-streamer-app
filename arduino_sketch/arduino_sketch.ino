// Define modem type
#define TINY_GSM_MODEM_SIM7600
#define TINY_GSM_USE_GPRS true
#define TINY_GSM_USE_WIFI false

// Libraries
#include <HardwareSerial.h>
#include <TinyGsmClient.h>
#include <SD.h>
#include <SPI.h>

// Modem settings
#define MODEM_TX 17
#define MODEM_RX 16
#define MODEM_BAUD 115200
HardwareSerial XCOM(1);
TinyGsm modem(XCOM);
TinyGsmClient client(modem);


// Server settings
String server = "9000-firebase-studio-1753223410587.cluster-73qgvk7hjjadkrjeyexca5ivva.cloudworkstations.dev";
int serverPort = 9002;
String endpoint = "/api/upload";

// SD card and file settings
#define SD_CS 5
#define CHUNK_SIZE 4096 // 4KB chunks
#define MAX_RETRIES 5
#define RETRY_DELAY_MS 2000

// Function to print modem debug information
void debugModemStatus() {
  Serial.println("--- Modem Status ---");

  // Check SIM status
  Serial.print("SIM status: ");
  Serial.println(modem.getSimStatus());

  // Check signal quality
  Serial.print("Signal quality: ");
  Serial.println(modem.getSignalQuality());

  // Check if registered to the network
  Serial.print("Network registration: ");
  Serial.println(modem.isNetworkRegistered());
  
  Serial.println("--------------------");
}


void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("🔌 Booting...");

  // Initialize SD card
  if (!SD.begin(SD_CS)) {
    Serial.println("❌ SD card failed. Halting.");
    while (true);
  }
  Serial.println("✅ SD card ready.");

  // Initialize modem
  Serial.println("📡 Initializing modem...");
  XCOM.begin(MODEM_BAUD, SERIAL_8N1, MODEM_RX, MODEM_TX);
  delay(3000);
  modem.restart();

  Serial.println("Connecting to network...");
  if (!modem.gprsConnect("vodafone", "", "")) {
     Serial.println("❌ Failed to connect to GPRS.");
  } else {
    Serial.println("✅ GPRS Connected.");
  }

  debugModemStatus();
  
  // Start file upload process
  sendFileChunks("/sigma2.wav");
}

void loop() {
  // Loop is empty, all work is done in setup for this example.
}


// Function to upload a file in chunks
void sendFileChunks(const char* filename) {
  File file = SD.open(filename, FILE_READ);
  if (!file) {
    Serial.println("❌ Failed to open file for reading.");
    return;
  }

  size_t totalSize = file.size();
  size_t offset = 0;
  int retryCount = 0;

  Serial.printf("📤 Sending file: %s (%d bytes) in %d byte chunks\n", filename, totalSize, CHUNK_SIZE);

  while (offset < totalSize) {
    if (retryCount >= MAX_RETRIES) {
        Serial.printf("❌ Aborting upload for %s after %d failed retries.\n", filename, MAX_RETRIES);
        break;
    }

    if (!client.connect(server.c_str(), serverPort)) {
        Serial.printf("❌ Connection to %s:%d failed. Retrying...\n", server.c_str(), serverPort);
        retryCount++;
        delay(RETRY_DELAY_MS);
        continue;
    }
    Serial.printf("✅ Connected to %s:%d\n", server.c_str(), serverPort);


    // Build HTTP request headers
    String headers = "POST " + endpoint + " HTTP/1.1\r\n";
    headers += "Host: " + server + "\r\n";
    headers += "X-Filename: " + String(filename) + "\r\n";
    headers += "X-Chunk-Offset: " + String(offset) + "\r\n";
    headers += "X-Total-Size: " + String(totalSize) + "\r\n";

    size_t chunkSize = min((size_t)CHUNK_SIZE, totalSize - offset);
    headers += "Content-Length: " + String(chunkSize) + "\r\n";
    headers += "Content-Type: application/octet-stream\r\n";
    headers += "\r\n";

    Serial.println("--- Sending Request ---");
    Serial.print(headers);
    Serial.printf("Body: %d bytes of binary data\n", chunkSize);
    Serial.println("-----------------------");


    // Send headers
    client.print(headers);

    // Send chunk data
    uint8_t buffer[CHUNK_SIZE];
    file.seek(offset);
    size_t bytesRead = file.read(buffer, chunkSize);
    client.write(buffer, bytesRead);
    client.flush();

    // Wait for server response
    unsigned long timeout = millis();
    bool responseOk = false;
    String responseStatus = "";
    String responseBody = "";

    while (millis() - timeout < 10000) { // 10 second timeout
      if (client.available()) {
        String line = client.readStringUntil('\n');
        line.trim();
        if (line.startsWith("HTTP/1.1")) {
          responseStatus = line;
          if (line.indexOf("200 OK") > -1 || line.indexOf("201 Created") > -1) {
            responseOk = true;
          }
        }
        // Read the rest of the response body if needed
        while(client.available()){
          responseBody += (char)client.read();
        }
        break; 
      }
    }
    
    Serial.println("--- Server Response ---");
    Serial.println("Status: " + (responseStatus.isEmpty() ? "No response" : responseStatus));
    Serial.println("Body: " + (responseBody.isEmpty() ? "Empty" : responseBody));
    Serial.println("-----------------------");

    client.stop();

    if (responseOk) {
      Serial.printf("✅ Chunk sent successfully (Offset: %d, Size: %d)\n", offset, bytesRead);
      offset += bytesRead;
      retryCount = 0; // Reset retry count on success
    } else {
      Serial.printf("❌ Chunk upload failed at offset %d. Retrying...\n", offset);
      retryCount++;
      delay(RETRY_DELAY_MS);
    }
     delay(500); // Small delay between chunks
  }

  file.close();
  if (offset == totalSize) {
    Serial.println("✅ File upload completed successfully.");
  } else {
    Serial.println("❌ File upload failed.");
  }
}