
#include <SPI.h>
#include <SD.h>
#include <HardwareSerial.h>

#define SD_CS 5          // SD card CS pin
#define SIM_RX 16        // SIM7600 TX -> ESP32 RX
#define SIM_TX 17        // SIM7600 RX -> ESP32 TX
#define MAX_RETRIES 3    // Number of times to retry a failed chunk
#define RETRY_DELAY 2000 // Delay between retries in milliseconds

HardwareSerial sim7600(1);

const char* apn = "vodafone";  // One NZ APN
// IMPORTANT: Replace this with your actual server URL
const char* serverUrl = "http://cellular-data-streamer.web.app/api/upload"; 

// Files on SD card to upload
const char* filesToSend[] = { "/sigma1.wav", "/sigma2.wav", "/sigma3.wav" };
const int numFiles = 3;

void setup() {
  Serial.begin(115200);
  sim7600.begin(115200, SERIAL_8N1, SIM_RX, SIM_TX);
  delay(3000);

  Serial.println("Initializing SD card...");
  if (!SD.begin(SD_CS)) {
    Serial.println("SD card init failed!");
    while (1);
  }
  Serial.println("✅ SD card initialized.");

  sendAT("AT", 1000);
  sendAT("AT+CGATT=1", 1000);
  sendAT("AT+CGDCONT=1,\"IP\",\"" + String(apn) + "\"", 1000);
  sendAT("AT+CGACT=1,1", 1000);
  sendAT("AT+HTTPTERM", 1000); // Terminate any existing HTTP session
  sendAT("AT+HTTPINIT", 1000);
  sendAT("AT+HTTPSSL=0", 1000); // Using HTTP, not HTTPS for this example
  sendAT("AT+HTTPPARA=\"CID\",1", 1000);
  sendAT("AT+HTTPPARA=\"UA\",\"ESP32-FileUpload-Agent\"", 1000);

  Serial.println("Ready to upload files. Enter file number or 'all':");
  printFileList();
}

void loop() {
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();

    if (input.equalsIgnoreCase("all")) {
      for (int i = 0; i < numFiles; i++) {
        uploadFile(filesToSend[i]);
      }
    } else {
      int fileIndex = input.toInt() - 1;
      if (fileIndex >= 0 && fileIndex < numFiles) {
        uploadFile(filesToSend[fileIndex]);
      } else {
        Serial.println("Invalid file number.");
      }
    }
    Serial.println("\nReady for next command.");
    printFileList();
  }
}

void uploadFile(const char* filename) {
  if (!SD.exists(filename)) {
    Serial.printf("File %s not found.\n", filename);
    return;
  }

  File f = SD.open(filename, FILE_READ);
  if (!f) {
    Serial.printf("Failed to open %s\n", filename);
    return;
  }

  size_t fileSize = f.size();
  Serial.printf("------ Starting Upload ------\n");
  Serial.printf("File: %s\nSize: %u bytes\n", filename, fileSize);
  Serial.println("--------------------------");


  const size_t chunkSize = 4096; // 4KB chunks
  size_t offset = 0;

  while (offset < fileSize) {
    bool chunkSuccess = false;
    for (int retry = 0; retry < MAX_RETRIES; retry++) {
      size_t currentChunkSize = min(chunkSize, fileSize - offset);
      
      Serial.printf("\nAttempting to send chunk: Offset=%u, Size=%u (Retry %d/%d)\n", offset, currentChunkSize, retry + 1, MAX_RETRIES);

      // We might need to re-init http on failure
      sendAT("AT+HTTPPARA=\"URL\",\"" + String(serverUrl) + "\"", 500);

      // Set headers for this chunk
      sim7600.println("AT+HTTPPARA=\"USERDATA\",\"x-filename: " + String(filename).substring(1) + "\"");
      if(!waitForOK(1000)) continue;
      sim7600.println("AT+HTTPPARA=\"USERDATA\",\"x-chunk-offset: " + String(offset) + "\"");
      if(!waitForOK(1000)) continue;
      sim7600.println("AT+HTTPPARA=\"USERDATA\",\"x-chunk-size: " + String(currentChunkSize) + "\"");
      if(!waitForOK(1000)) continue;
      sim7600.println("AT+HTTPPARA=\"USERDATA\",\"x-total-size: " + String(fileSize) + "\"");
      if(!waitForOK(1000)) continue;

      sendAT("AT+HTTPDATA=" + String(currentChunkSize) + ",10000", 500);
      if (!waitForPrompt(5000)) {
        Serial.println("⏱ Timeout waiting for '>' prompt. Retrying...");
        sendAT("AT+HTTPTERM", 1000);
        sendAT("AT+HTTPINIT", 1000);
        delay(RETRY_DELAY);
        continue;
      }

      // Read chunk from SD and send to modem
      uint8_t buffer[currentChunkSize];
      f.seek(offset);
      f.read(buffer, currentChunkSize);
      sim7600.write(buffer, currentChunkSize);

      if (!waitForOK(10000)) {
        Serial.println("⏱ Timeout waiting for OK after data. Retrying...");
        sendAT("AT+HTTPTERM", 1000);
        sendAT("AT+HTTPINIT", 1000);
        delay(RETRY_DELAY);
        continue;
      }
      
      // Send POST request
      sendAT("AT+HTTPACTION=1", 500);
      String httpResponse = readResponse(20000); // Wait up to 20s for response
      
      Serial.println("--- Server Response ---");
      Serial.println(httpResponse.length() > 0 ? httpResponse : "No response from server.");
      Serial.println("-----------------------");


      if (httpResponse.indexOf("+HTTPACTION: 1,200") != -1 || httpResponse.indexOf("+HTTPACTION: 1,201") != -1) {
        Serial.printf("✅ Chunk success. Offset: %u\n", offset);
        chunkSuccess = true;
        break; // Exit retry loop
      } else {
        Serial.printf("❌ Chunk failed. HTTP Status not 200/201. Retrying in %dms...\n", RETRY_DELAY);
        sendAT("AT+HTTPTERM", 1000);
        sendAT("AT+HTTPINIT", 1000);
        delay(RETRY_DELAY);
      }
    }

    if (!chunkSuccess) {
      Serial.println("\n🚨 CRITICAL: Failed to upload chunk after multiple retries. Aborting file upload.");
      f.close();
      return; // Abort for this file
    }

    offset += chunkSize;
  }

  f.close();
  Serial.println("\n🎉🎉🎉 Upload complete. 🎉🎉🎉");
}

void printFileList() {
  Serial.println("--- Files on SD card ---");
  for (int i = 0; i < numFiles; i++) {
    if (SD.exists(filesToSend[i])) {
      File f = SD.open(filesToSend[i]);
      Serial.printf("%d: %s (%u bytes)\n", i + 1, filesToSend[i], f.size());
      f.close();
    } else {
      Serial.printf("%d: %s (Not Found)\n", i + 1, filesToSend[i]);
    }
  }
   Serial.println("------------------------");
}

String readResponse(unsigned int timeout) {
    String response = "";
    unsigned long start = millis();
    while (millis() - start < timeout) {
        if (sim7600.available()) {
            response += (char)sim7600.read();
        }
    }
    return response;
}


void sendAT(String cmd, unsigned int timeout) {
  Serial.print(">> ");
  Serial.println(cmd);
  sim7600.println(cmd);
  String response = readResponse(timeout);
  Serial.print("<< ");
  Serial.println(response.length() > 0 ? response.substring(0, response.length()-2) : "[No Response]"); // trim trailing \r\n
}


bool waitForPrompt(unsigned int timeout) {
  unsigned long start = millis();
  String response;
  while (millis() - start < timeout) {
    if (sim7600.available()) {
      char c = sim7600.read();
      // Serial.write(c); // Suppress echoing the prompt for cleaner logs
      if (c == '>') return true;
    }
  }
  return false;
}

bool waitForOK(unsigned int timeout) {
  unsigned long start = millis();
  String response;
  while (millis() - start < timeout) {
    if (sim7600.available()) {
      response += (char)sim7600.read();
      if (response.indexOf("OK") >= 0) {
        // Serial.println("DEBUG (waitForOK): " + response);
        return true;
      }
      if (response.indexOf("ERROR") >= 0){
        // Serial.println("DEBUG (waitForOK): " + response);
         return false;
      }
    }
  }
  return false;
}

    