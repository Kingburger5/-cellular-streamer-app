// Define the serial ports
#define SerialMon Serial
#define SerialAT Serial1

// Pin definitions
#define SIM7600_TX 27
#define SIM7600_RX 26
#define SD_CS      5

// Your network and Firebase configuration
const char apn[] = "internet"; // Replace with your APN
const char firebase_host[] = "firebasestorage.googleapis.com";
const char firebase_bucket[] = "cellular-data-streamer.appspot.com";

// Include necessary libraries
#include "FS.h"
#include "SD.h"
#include "SPI.h"

// Function to send AT command and wait for a specific response
bool sendATCommand(const char* cmd, const char* expect_response, unsigned int timeout) {
  unsigned long start_time = millis();
  SerialAT.println(cmd);
  SerialMon.print(">> ");
  SerialMon.println(cmd);

  String response = "";
  while (millis() - start_time < timeout) {
    if (SerialAT.available()) {
      char c = SerialAT.read();
      response += c;
      SerialMon.write(c);
    }
    if (response.indexOf(expect_response) != -1) {
      return true;
    }
  }
  SerialMon.println("\n[ERROR] Timeout or unexpected response.");
  return false;
}

// Function to initialize the SD card
bool initSDCard() {
  SerialMon.println("Initializing SD card...");
  if (!SD.begin(SD_CS)) {
    SerialMon.println("[ERROR] SD Card Mount Failed");
    return false;
  }
  uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE) {
    SerialMon.println("[ERROR] No SD card attached");
    return false;
  }
  SerialMon.println("SD card initialized.");
  return true;
}

// Function to initialize and connect the SIM7600G module
bool initSIM7600() {
  SerialMon.println("Initializing SIM7600G...");

  SerialAT.begin(115200, SERIAL_8N1, SIM7600_RX, SIM7600_TX);
  delay(1000);

  if (!sendATCommand("AT", "OK", 2000)) return false;
  if (!sendATCommand("ATE0", "OK", 2000)) return false; // Disable echo
  if (!sendATCommand("AT+CPIN?", "READY", 5000)) return false; // Check SIM
  if (!sendATCommand("AT+CSQ", "OK", 2000)) return false; // Check signal quality
  if (!sendATCommand("AT+CGREG?", "+CGREG: 0,1", 10000)) return false; // Check network registration
  if (!sendATCommand("AT+COPS?", "OK", 10000)) return false; // Check operator
  
  String cmd = "AT+CGDCONT=1,\"IP\",\"" + String(apn) + "\"";
  if (!sendATCommand(cmd.c_str(), "OK", 5000)) return false; // Set APN
  
  if (!sendATCommand("AT+NETOPEN", "OK", 10000)) return false; // Open network
  if (!sendATCommand("AT+IPADDR", "+IPADDR:", 5000)) return false; // Get IP Address

  SerialMon.println("SIM7600G Initialized and Connected to Network.");
  return true;
}

void uploadFile(const char* filename) {
  File file = SD.open(filename, FILE_READ);
  if (!file) {
    SerialMon.println("[ERROR] Failed to open file for reading");
    return;
  }
  size_t fileSize = file.size();
  SerialMon.print("Starting upload for: ");
  SerialMon.print(filename);
  SerialMon.print(", Size: ");
  SerialMon.println(fileSize);

  // 1. Initialize HTTP Service
  if (!sendATCommand("AT+HTTPINIT", "OK", 10000)) {
    file.close();
    return;
  }

  // 2. Set HTTP Parameters
  String url = "https://" + String(firebase_host) + "/v0/b/" + String(firebase_bucket) + "/o" + String(filename) + "?uploadType=media&name=" + String(filename).substring(1);
  String cmd = "AT+HTTPPARA=\"URL\",\"" + url + "\"";
  if (!sendATCommand(cmd.c_str(), "OK", 5000)) {
    sendATCommand("AT+HTTPTERM", "OK", 5000);
    file.close();
    return;
  }
  if (!sendATCommand("AT+HTTPPARA=\"CONTENT\",\"audio/wav\"", "OK", 5000)) {
     sendATCommand("AT+HTTPTERM", "OK", 5000);
     file.close();
     return;
  }

  // 3. Set HTTP Data
  cmd = "AT+HTTPDATA=" + String(fileSize) + ",120000"; // 120s timeout
  if (!sendATCommand(cmd.c_str(), "DOWNLOAD", 10000)) {
    sendATCommand("AT+HTTPTERM", "OK", 5000);
    file.close();
    return;
  }

  // 4. Send File Data
  SerialMon.println("Sending file data...");
  uint8_t buffer[256];
  size_t bytes_sent = 0;
  while (file.available()) {
    size_t bytes_to_read = file.read(buffer, sizeof(buffer));
    SerialAT.write(buffer, bytes_to_read);
    bytes_sent += bytes_to_read;
    SerialMon.print(".");
  }
  SerialMon.println("\nFile data sent.");
  
  // Wait for OK confirmation after data is sent
  unsigned long start_time = millis();
  String response = "";
  while (millis() - start_time < 120000) { // Long timeout for upload
      if (SerialAT.available()) {
          char c = SerialAT.read();
          response += c;
          if (response.indexOf("OK") != -1) break;
          if (response.indexOf("ERROR") != -1) break;
      }
  }
  SerialMon.println("Modem response after data: " + response);


  // 5. Send POST Request
  if (!sendATCommand("AT+HTTPACTION=1", "+HTTPACTION: 1,200", 120000)) {
    SerialMon.println("[ERROR] Upload failed.");
  } else {
    SerialMon.println("Upload successful!");
  }

  // 6. Terminate HTTP Service
  sendATCommand("AT+HTTPTERM", "OK", 5000);
  file.close();
}


void setup() {
  SerialMon.begin(115200);
  while (!SerialMon);

  SerialMon.println("\n--- ESP32 SIM7600G Firebase Uploader ---");

  if (!initSDCard()) {
    SerialMon.println("HALTED: SD Card Error.");
    while (1);
  }

  if (!initSIM7600()) {
    SerialMon.println("HALTED: SIM7600G Error.");
    while (1);
  }
  
  SerialMon.println("System Initialized. Ready for upload.");
}

void loop() {
  File root = SD.open("/");
  if (!root) {
    SerialMon.println("[ERROR] Failed to open root directory.");
    delay(5000);
    return;
  }

  SerialMon.println("\n--- Select a .wav file to upload ---");
  int fileCount = 0;
  File entry = root.openNextFile();
  while (entry) {
    if (!entry.isDirectory() && String(entry.name()).endsWith(".wav")) {
      fileCount++;
      SerialMon.print(fileCount);
      SerialMon.print(": ");
      SerialMon.println(entry.name());
    }
    entry.close();
    entry = root.openNextFile();
  }

  if (fileCount == 0) {
    SerialMon.println("No .wav files found on SD card.");
    delay(10000);
    return;
  }
  
  SerialMon.print("Enter file number to upload (1-" + String(fileCount) + "): ");
  while (!SerialMon.available());

  int fileNumber = SerialMon.parseInt();
  if (fileNumber > 0 && fileNumber <= fileCount) {
    // Find the selected file again
    root.rewindDirectory();
    int currentFile = 0;
    String fileToUpload = "";
    entry = root.openNextFile();
    while (entry) {
      if (!entry.isDirectory() && String(entry.name()).endsWith(".wav")) {
        currentFile++;
        if (currentFile == fileNumber) {
          fileToUpload = entry.name();
          break;
        }
      }
      entry.close();
      entry = root.openNextFile();
    }
    
    if (fileToUpload != "") {
      uploadFile(fileToUpload.c_str());
    } else {
      SerialMon.println("Invalid file selection.");
    }
  } else {
    SerialMon.println("Invalid number. Please try again.");
  }

  // Clear serial buffer
  while(SerialMon.available()) {
    SerialMon.read();
  }
  
  SerialMon.println("\n--- Task finished. Ready for next file. ---");
  delay(5000);
}
