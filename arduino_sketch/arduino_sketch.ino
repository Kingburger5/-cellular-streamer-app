#include <SPI.h>
#include <SD.h>
#include "FS.h"

// ESP32-specific: Use HardwareSerial for the modem
// Serial1 is usually not available on all ESP32 boards (used for flash)
// We will use Serial2 for the modem
HardwareSerial modem(2);

// Define the pins for the SIM7600G module
#define SIM7600_TX 27
#define SIM7600_RX 26

// Define the Chip Select pin for the SD Card
#define SD_CS 5

// Your network and Firebase configuration
const char* APN = "internet"; // Replace with your APN
const char* FIREBASE_HOST = "firebasestorage.googleapis.com";
const char* BUCKET_NAME = "cellular-data-streamer.appspot.com";

/**
 * @brief Sends an AT command to the modem and waits for a response.
 * 
 * @param command The AT command to send.
 * @param timeout The time to wait for a response in milliseconds.
 * @param expected_response The expected successful response string (e.g., "OK", "DOWNLOAD").
 * @return String The full response from the modem.
 */
String sendATCommand(const char* command, unsigned long timeout, const char* expected_response) {
    String response = "";
    unsigned long startTime = millis();

    Serial.print(">> ");
    Serial.println(command);

    modem.println(command);

    while (millis() - startTime < timeout) {
        if (modem.available()) {
            String line = modem.readStringUntil('\n');
            line.trim();
            response += line + "\n";
            if (line.indexOf(expected_response) != -1) {
                break; 
            }
             if (line.indexOf("ERROR") != -1) {
                break;
            }
        }
    }
    
    // Clean up the response for better logging
    response.trim();
    if (response.length() > 0) {
      Serial.println(response);
    } else {
      Serial.println("[INFO] No response from modem.");
    }
    
    // Check if the final response contains the expected string
    if (response.indexOf(expected_response) == -1) {
        Serial.println("[ERROR] Timeout or unexpected response.");
        response = "ERROR"; // Set response to "ERROR" to indicate failure
    }

    return response;
}


/**
 * @brief Initializes and prepares the SIM7600G module for communication.
 * @return bool True if initialization is successful, false otherwise.
 */
bool setupSIM7600() {
    Serial.println("--- Initializing SIM7600G ---");

    if (sendATCommand("AT", 2000, "OK") == "ERROR") return false;
    if (sendATCommand("ATE0", 2000, "OK") == "ERROR") return false; // Disable command echo
    if (sendATCommand("AT+CPIN?", 5000, "+CPIN: READY") == "ERROR") return false;
    if (sendATCommand("AT+CSQ", 5000, "OK") == "ERROR") return false; // Check signal quality
    
    // Wait for network registration (up to 30 seconds)
    unsigned long start = millis();
    bool registered = false;
    while (millis() - start < 30000) {
        String response = sendATCommand("AT+CGREG?", 2000, "OK");
        if (response.indexOf("+CGREG: 0,1") != -1 || response.indexOf("+CGREG: 0,5") != -1) {
            Serial.println("[INFO] Registered to network.");
            registered = true;
            break;
        }
        delay(2000);
    }
    if (!registered) {
       Serial.println("[ERROR] Failed to register to network.");
       return false;
    }

    if (sendATCommand("AT+COPS?", 5000, "OK") == "ERROR") return false; // Check operator
    if (sendATCommand("AT+CGDCONT=1,\"IP\",\"", 5000, "OK") == "ERROR") return false;
    if (sendATCommand("AT+NETOPEN", 10000, "OK") == "ERROR") return false;
    if (sendATCommand("AT+IPADDR", 5000, "OK") == "ERROR") return false;

    Serial.println("--- SIM7600G Initialized Successfully ---");
    return true;
}

/**
 * @brief Lists all .wav files in the root directory of the SD card.
 */
void listWavFiles() {
    Serial.println("--- Scanning for .wav files on SD Card ---");
    File root = SD.open("/");
    if (!root) {
        Serial.println("[ERROR] Failed to open root directory.");
        return;
    }
    if (!root.isDirectory()) {
        Serial.println("[ERROR] Root is not a directory.");
        return;
    }

    File file = root.openNextFile();
    int fileCount = 0;
    while (file) {
        if (!file.isDirectory() && String(file.name()).endsWith(".wav")) {
            fileCount++;
            Serial.print(fileCount);
            Serial.print(": ");
            Serial.print(file.name());
            Serial.print("  (");
            Serial.print(file.size());
            Serial.println(" bytes)");
        }
        file.close();
        file = root.openNextFile();
    }

    if (fileCount == 0) {
        Serial.println("No .wav files found.");
    }
    root.close();
}


/**
 * @brief Uploads a specified file from the SD card to Firebase Storage.
 * 
 * @param filename The name of the file to upload.
 */
void uploadFileToFirebase(const String& filename) {
    if (!SD.exists(filename)) {
        Serial.println("[ERROR] File does not exist: " + filename);
        return;
    }

    File fileToUpload = SD.open(filename, FILE_READ);
    if (!fileToUpload) {
        Serial.println("[ERROR] Failed to open file for reading: " + filename);
        return;
    }

    size_t fileSize = fileToUpload.size();
    Serial.print("--- Starting Upload for: ");
    Serial.print(filename);
    Serial.print(", Size: ");
    Serial.print(fileSize);
    Serial.println(" bytes ---");

    // 1. Initialize HTTP service
    if (sendATCommand("AT+HTTPINIT", 5000, "OK") == "ERROR") {
        fileToUpload.close();
        return;
    }

    // 2. Set HTTP parameters
    String url = "https://";
    url += FIREBASE_HOST;
    url += "/v0/b/";
    url += BUCKET_NAME;
    url += "/o/";
    url += filename;
    url += "?uploadType=media&name=";
    url += filename;

    String httpUrlCmd = "AT+HTTPPARA=\"URL\",\"" + url + "\"";
    if (sendATCommand(httpUrlCmd.c_str(), 5000, "OK") == "ERROR") {
        sendATCommand("AT+HTTPTERM", 3000, "OK");
        fileToUpload.close();
        return;
    }
    
    if (sendATCommand("AT+HTTPPARA=\"CONTENT\",\"audio/wav\"", 5000, "OK") == "ERROR") {
        sendATCommand("AT+HTTPTERM", 3000, "OK");
        fileToUpload.close();
        return;
    }

    // 3. Set HTTP data length and wait for DOWNLOAD prompt
    String httpDataCmd = "AT+HTTPDATA=" + String(fileSize) + ",10000"; // file size, 10s timeout
    if (sendATCommand(httpDataCmd.c_str(), 10000, "DOWNLOAD") == "ERROR") {
        sendATCommand("AT+HTTPTERM", 3000, "OK");
        fileToUpload.close();
        return;
    }

    // 4. Send file data
    Serial.println("[INFO] Sending file content...");
    byte buffer[256];
    size_t bytesRead = 0;
    while ((bytesRead = fileToUpload.read(buffer, sizeof(buffer))) > 0) {
        modem.write(buffer, bytesRead);
    }
    fileToUpload.close();
    Serial.println("\n[INFO] File content sent.");
    
    // After sending data, wait for OK confirmation from module
    String dataResponse = modem.readString();
    if(dataResponse.indexOf("OK") == -1){
        Serial.println("[ERROR] Did not receive OK after sending file data.");
        sendATCommand("AT+HTTPTERM", 3000, "OK");
        return;
    }
    
    // 5. Send POST request
    if (sendATCommand("AT+HTTPACTION=1", 30000, "+HTTPACTION: 1,200") == "ERROR") {
         Serial.println("[ERROR] HTTP POST failed or non-200 response.");
    } else {
        Serial.println("[SUCCESS] File uploaded successfully!");
    }

    // 6. Terminate HTTP service
    sendATCommand("AT+HTTPTERM", 3000, "OK");
    sendATCommand("AT+NETCLOSE", 5000, "OK");
    
    Serial.println("--- Upload Process Finished ---");
}

/**
 * @brief Gets the nth .wav filename from the SD card.
 * 
 * @param index The 1-based index of the file to find.
 * @return String The filename, or an empty string if not found.
 */
String getWavFilenameByIndex(int index) {
    File root = SD.open("/");
    if (!root || !root.isDirectory()) return "";

    File file = root.openNextFile();
    int count = 0;
    String filename = "";

    while(file) {
        if (!file.isDirectory() && String(file.name()).endsWith(".wav")) {
            count++;
            if (count == index) {
                filename = String(file.name());
                break;
            }
        }
        file.close();
        file = root.openNextFile();
    }
    
    root.close();
    return filename;
}


void setup() {
    // Start Serial Monitor
    Serial.begin(115200);
    while (!Serial); // Wait for serial port to connect
    Serial.println("\n--- ESP32 Cellular Data Logger ---");

    // Initialize the modem serial port
    modem.begin(115200, SERIAL_8N1, SIM7600_RX, SIM7600_TX);
    Serial.println("Modem serial initialized.");

    // Initialize SD Card
    Serial.println("--- Initializing SD Card ---");
    if (!SD.begin(SD_CS)) {
        Serial.println("[ERROR] SD Card Mount Failed. Halting.");
        while (1);
    }
    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    Serial.print("SD Card Size: ");
    Serial.print(cardSize);
    Serial.println(" MB");

    // Initialize SIM7600
    if (!setupSIM7600()) {
        Serial.println("HALTED: SIM7600G Error.");
        while (1);
    }

    // List files and prompt user
    listWavFiles();
    Serial.println("\nEnter the number of the file to upload:");
}

void loop() {
    if (Serial.available()) {
        String input = Serial.readStringUntil('\n');
        input.trim();
        int fileNumber = input.toInt();

        if (fileNumber > 0) {
            String filenameToUpload = getWavFilenameByIndex(fileNumber);
            if (filenameToUpload.length() > 0) {
                Serial.print("Preparing to upload: ");
                Serial.println(filenameToUpload);
                uploadFileToFirebase(filenameToUpload);
            } else {
                Serial.println("Invalid file number.");
            }
        } else {
            Serial.println("Invalid input. Please enter a number.");
        }
         Serial.println("\nEnter the number of the file to upload:");
    }
}
