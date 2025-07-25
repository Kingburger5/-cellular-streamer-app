#include <SPI.h>
#include <SD.h>
#include <HardwareSerial.h>

// Pin definitions
#define SIM7600_TX 17
#define SIM7600_RX 16
#define SD_CS      5

// APN for your mobile network provider
String apn = "internet"; 

// Firebase Storage Bucket URL
String bucketUrl = "cellular-data-streamer.appspot.com";

// Use Serial2 for SIM7600G
HardwareSerial modem(2);

// =================================================================
//                        HELPER FUNCTIONS
// =================================================================

/**
 * Sends an AT command to the modem and waits for a specific response.
 * This version reads line by line to avoid buffer overflows and garbled output.
 * @param cmd The AT command to send.
 * @param expectedResponse The string to expect in the modem's response.
 * @param timeout The maximum time to wait for the response in milliseconds.
 * @return True if the expected response is received, false otherwise.
 */
bool sendATCommand(String cmd, String expectedResponse, unsigned long timeout) {
    Serial.print(">> ");
    Serial.println(cmd);
    
    // Clear any previous data in the modem serial buffer
    while(modem.available()) {
        modem.read();
    }
    
    modem.println(cmd);

    unsigned long startTime = millis();
    String response = "";
    
    while (millis() - startTime < timeout) {
        if (modem.available()) {
            // Read line by line
            String line = modem.readStringUntil('\n');
            line.trim(); // Remove leading/trailing whitespace
            if (line.length() > 0) {
                 Serial.println(line); // Print each line for debugging
                 response += line;
            }
        }
        // Check if the full concatenated response contains what we want
        if (response.indexOf(expectedResponse) != -1) {
            return true;
        }
    }
    
    Serial.println("[ERROR] Timeout or unexpected response.");
    return false;
}

/**
 * Initializes the SIM7600G module with a sequence of AT commands.
 * @return True if initialization is successful, false otherwise.
 */
bool setupSIM7600() {
    Serial.println("[INFO] Initializing SIM7600G...");

    if (!sendATCommand("AT", "OK", 2000)) return false;
    if (!sendATCommand("ATE0", "OK", 2000)) return false; // Disable echo
    if (!sendATCommand("AT+CPIN?", "READY", 5000)) return false;
    if (!sendATCommand("AT+CSQ", "OK", 2000)) return false;

    // Wait for network registration (up to 30 seconds)
    Serial.println("[INFO] Waiting for network registration...");
    if (!sendATCommand("AT+CGREG?", "+CGREG: 0,1", 30000) && !sendATCommand("AT+CGREG?", "+CGREG: 0,5", 30000)) {
         Serial.println("[ERROR] Failed to register to network.");
        return false;
    }
    Serial.println("[INFO] Registered to network.");

    if (!sendATCommand("AT+COPS?", "OK", 5000)) return false;

    // Set APN
    String cmd = "AT+CGDCONT=1,\"IP\",\"" + apn + "\"";
    if (!sendATCommand(cmd, "OK", 10000)) {
        Serial.println("[ERROR] Failed to set APN.");
        return false;
    }
    
    // Open network
    if (!sendATCommand("AT+NETOPEN", "Network opened", 20000)) {
        Serial.println("[ERROR] Failed to open network.");
        return false;
    }

    // Check IP address
    if (!sendATCommand("AT+IPADDR", "+IPADDR:", 5000)) {
         Serial.println("[ERROR] Failed to get IP address.");
        return false;
    }

    Serial.println("[INFO] SIM7600G Initialized Successfully.");
    return true;
}

/**
 * Uploads a file from the SD card to Firebase Storage via HTTPS POST.
 * @param filename The name of the file to upload.
 * @return True if the upload is successful, false otherwise.
 */
bool uploadFileToFirebase(String filename) {
    Serial.println("[INFO] Starting file upload for: " + filename);

    File file = SD.open("/" + filename);
    if (!file) {
        Serial.println("[ERROR] Failed to open file: " + filename);
        return false;
    }
    long fileSize = file.size();
    Serial.println("[INFO] File size: " + String(fileSize) + " bytes");

    // 1. Initialize HTTP service
    if (!sendATCommand("AT+HTTPINIT", "OK", 10000)) {
        file.close();
        return false;
    }

    // 2. Set HTTP parameters
    String url = "https://firebasestorage.googleapis.com/v0/b/" + bucketUrl + "/o/" + filename + "?uploadType=media&name=" + filename;
    if (!sendATCommand("AT+HTTPPARA=\"URL\",\"" + url + "\"", "OK", 10000)) {
        file.close();
        return false;
    }
    if (!sendATCommand("AT+HTTPPARA=\"CONTENT\",\"audio/wav\"", "OK", 5000)) {
        file.close();
        return false;
    }

    // 3. Set HTTP data size and wait for DOWNLOAD prompt
    if (!sendATCommand("AT+HTTPDATA=" + String(fileSize) + ",120000", "DOWNLOAD", 10000)) {
        Serial.println("[ERROR] Failed to set HTTP data size.");
        file.close();
        return false;
    }

    // 4. Send file data
    Serial.println("[INFO] Sending file data...");
    byte buffer[64];
    while (file.available()) {
        int bytesRead = file.read(buffer, sizeof(buffer));
        modem.write(buffer, bytesRead);
    }
    file.close();
    Serial.println("[INFO] File data sent.");

    // Wait for OK after data transmission
    if (!sendATCommand("", "OK", 120000)) { // Increased timeout for upload
        Serial.println("[ERROR] Did not receive OK after sending data.");
        return false;
    }

    // 5. Send POST request
    if (!sendATCommand("AT+HTTPACTION=1", "+HTTPACTION: 1,200", 120000)) {
        Serial.println("[ERROR] HTTP POST request failed.");
        return false;
    }

    Serial.println("[SUCCESS] File uploaded successfully!");

    // 6. Terminate HTTP service
    sendATCommand("AT+HTTPTERM", "OK", 5000);

    return true;
}


// =================================================================
//                           SETUP
// =================================================================
void setup() {
    // Start serial communication for debugging
    Serial.begin(115200);
    while (!Serial); // Wait for serial port to connect

    Serial.println("\n== Cellular Data Streamer ==");

    // Initialize modem serial port
    modem.begin(115200, SERIAL_8N1, SIM7600_RX, SIM7600_TX);
    Serial.println("[INFO] Modem serial port initialized.");

    // Initialize SD Card
    Serial.println("[INFO] Initializing SD card...");
    if (!SD.begin(SD_CS)) {
        Serial.println("[ERROR] SD Card initialization failed!");
        while (1); // Halt
    }
    Serial.println("[INFO] SD card initialized.");

    // List .wav files
    Serial.println("\n[INFO] Searching for .wav files on SD Card...");
    File root = SD.open("/");
    if (!root) {
        Serial.println("[ERROR] Failed to open root directory.");
        while(1);
    }
    
    File file = root.openNextFile();
    int fileCount = 0;
    String fileList[50]; // Max 50 wav files

    while (file) {
        String fileName = file.name();
        if (fileName.endsWith(".wav") || fileName.endsWith(".WAV")) {
            if (fileCount < 50) {
                 fileList[fileCount] = fileName;
                 fileCount++;
            }
        }
        file.close();
        file = root.openNextFile();
    }
    root.close();

    if (fileCount == 0) {
        Serial.println("[INFO] No .wav files found.");
        while (1); // Halt
    }

    Serial.println("\nSelect a file to upload:");
    for (int i = 0; i < fileCount; i++) {
        Serial.println(String(i + 1) + ": " + fileList[i]);
    }
    
    // Wait for user input
    int selectedFileIndex = -1;
    while (selectedFileIndex == -1) {
        if (Serial.available() > 0) {
            String input = Serial.readStringUntil('\n');
            input.trim();
            int num = input.toInt();
            if (num > 0 && num <= fileCount) {
                selectedFileIndex = num - 1;
            } else {
                Serial.println("Invalid selection. Please try again.");
            }
        }
    }
    String selectedFilename = fileList[selectedFileIndex];
    Serial.println("[INFO] You selected: " + selectedFilename);


    // Initialize SIM7600G
    if (!setupSIM7600()) {
        Serial.println("HALTED: SIM7600G Error.");
        while (1); // Halt on failure
    }
    
    // Upload the selected file
    if (uploadFileToFirebase(selectedFilename)) {
        Serial.println("[INFO] Upload process finished.");
    } else {
        Serial.println("[INFO] Upload process failed.");
    }

    // Cleanup network connection
    Serial.println("[INFO] Closing network connection.");
    sendATCommand("AT+NETCLOSE", "OK", 10000);
    
    Serial.println("[INFO] Task complete. Entering deep sleep for 10 seconds.");
    esp_sleep_enable_timer_wakeup(10 * 1000000);
    esp_deep_sleep_start();
}


// =================================================================
//                           LOOP
// =================================================================
void loop() {
    // The ESP32 will restart from setup() after deep sleep.
    // The loop is intentionally left empty.
}
