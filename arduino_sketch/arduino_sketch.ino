#include <SPI.h>
#include <SD.h>
#include <HardwareSerial.h>

// Essential Pin Definitions
#define SD_CS 5       // SD card Chip Select
#define SIM7600_TX 27 // SIM7600G TX to ESP32 RX
#define SIM7600_RX 26 // SIM7600G RX to ESP32 TX
#define PWR_KEY 4     // Pin to power on/off the SIM7600G module

// APN for your mobile network provider
const char* APN = "internet"; // Replace with your APN

// Firebase Storage Bucket
const char* FIREBASE_HOST = "firebasestorage.googleapis.com";
const char* BUCKET_NAME = "cellular-data-streamer.appspot.com";

HardwareSerial simSerial(1); // Use UART 1 for SIM7600G

void setup() {
    Serial.begin(115200);
    while (!Serial) {
        ; // wait for serial port to connect.
    }
    Serial.println("--- ESP32 Firebase WAV Uploader ---");

    // Initialize SIM7600G Serial
    simSerial.begin(115200, SERIAL_8N1, SIM7600_RX, SIM7600_TX);

    // Power on the SIM7600G module
    pinMode(PWR_KEY, OUTPUT);
    digitalWrite(PWR_KEY, LOW);
    delay(1000);
    digitalWrite(PWR_KEY, HIGH);
    delay(2000);
    digitalWrite(PWR_KEY, LOW);
    
    Serial.println("Initializing SD Card...");
    if (!SD.begin(SD_CS)) {
        Serial.println("SD Card initialization failed!");
        while (1);
    }
    Serial.println("SD Card initialized.");

    if (!setupSIM7600()) {
        Serial.println("HALTED: SIM7600G Error.");
        while(1);
    }

    // Main loop will be called after setup
}

void loop() {
    File root = SD.open("/");
    if (!root) {
        Serial.println("Failed to open directory");
        return;
    }

    String files[50]; // Array to store wav file names
    int fileCount = 0;

    Serial.println("\nScanning for .wav files on SD card...");
    
    File file = root.openNextFile();
    while(file && fileCount < 50){
        String fileName = file.name();
        if (fileName.endsWith(".wav") || fileName.endsWith(".WAV")) {
            files[fileCount++] = fileName;
        }
        file.close();
        file = root.openNextFile();
    }
    root.close();

    if (fileCount == 0) {
        Serial.println("No .wav files found on SD card.");
        delay(10000); // Wait before scanning again
        return;
    }

    Serial.println("Available .wav files:");
    for (int i = 0; i < fileCount; i++) {
        Serial.printf("%d: %s\n", i + 1, files[i].c_str());
    }

    int choice = 0;
    while (choice < 1 || choice > fileCount) {
        Serial.printf("\nEnter a file number to upload (1-%d): ", fileCount);
        while (Serial.available() == 0) {
            // Wait for user input
        }
        String input = Serial.readStringUntil('\n');
        choice = input.toInt();
        if (choice < 1 || choice > fileCount) {
            Serial.println("Invalid choice. Please try again.");
        }
    }

    String filenameToUpload = files[choice - 1];
    Serial.printf("\nPreparing to upload: %s\n", filenameToUpload.c_str());

    // Attempt to upload the selected file
    uploadFileToFirebase(filenameToUpload);

    Serial.println("\n----------------------------------------");
    Serial.println("Upload process finished. Ready for next file.");
    Serial.println("----------------------------------------\n");
    delay(5000);
}


String sendATCommand(const char* cmd, unsigned long timeout, const char* expected_response) {
    String response = "";
    unsigned long startTime = millis();

    simSerial.println(cmd);
    Serial.print(">> ");
    Serial.println(cmd);

    while (millis() - startTime < timeout) {
        if (simSerial.available()) {
            String line = simSerial.readStringUntil('\n');
            line.trim(); // Remove any leading/trailing whitespace
            if (line.length() > 0) {
                Serial.println(line);
                response += line + "\n";
                if (strstr(line.c_str(), expected_response)) {
                    return response;
                }
            }
        }
    }
    Serial.println("\n[ERROR] Timeout or unexpected response.");
    return ""; // Return empty string on timeout or error
}

bool setupSIM7600() {
    Serial.println("--- Initializing SIM7600G Module ---");
    
    if (sendATCommand("AT", 2000, "OK") == "") return false;
    if (sendATCommand("ATE0", 2000, "OK") == "") return false; // Disable echo
    if (sendATCommand("AT+CPIN?", 5000, "+CPIN: READY") == "") return false;

    // Check signal quality
    if (sendATCommand("AT+CSQ", 5000, "OK") == "") return false;
    
    Serial.println("Waiting for network registration...");
    // Wait up to 30 seconds for network registration
    if (sendATCommand("AT+CGREG?", 30000, "+CGREG: 0,1") == "") { 
        Serial.println("Failed to register on the network.");
        return false;
    }
    Serial.println("Registered on network.");

    if (sendATCommand("AT+COPS?", 5000, "OK") == "") return false;

    Serial.println("--- Activating PDP Context ---");
    String cmd = "AT+CGDCONT=1,\"IP\",\"" + String(APN) + "\"";
    if (sendATCommand(cmd.c_str(), 10000, "OK") == "") return false;
    
    if (sendATCommand("AT+NETOPEN", 20000, "+NETOPEN: 0") == "") {
        Serial.println("Failed to open network. Already open?");
        // It might be already open, let's check the IP.
    }

    if (sendATCommand("AT+IPADDR", 10000, "+IPADDR:") == "") {
        Serial.println("Failed to get IP address.");
        return false;
    }
    
    Serial.println("--- SIM7600G Ready ---");
    return true;
}

void uploadFileToFirebase(String filename) {
    if (sendATCommand("AT+HTTPINIT", 10000, "OK") == "") {
        Serial.println("Failed to initialize HTTP service.");
        return;
    }

    String url = "https://" + String(FIREBASE_HOST) + "/v0/b/" + String(BUCKET_NAME) + "/o/" + filename + "?uploadType=media&name=" + filename;
    String httpUrlCmd = "AT+HTTPPARA=\"URL\",\"" + url + "\"";
    if (sendATCommand(httpUrlCmd.c_str(), 10000, "OK") == "") return;

    if (sendATCommand("AT+HTTPPARA=\"CONTENT\",\"audio/wav\"", 10000, "OK") == "") return;

    File file = SD.open("/" + filename, FILE_READ);
    if (!file) {
        Serial.println("Failed to open file for reading.");
        sendATCommand("AT+HTTPTERM", 10000, "OK"); // Terminate HTTP
        return;
    }

    size_t fileSize = file.size();
    Serial.printf("File size: %d bytes\n", fileSize);

    String httpDataCmd = "AT+HTTPDATA=" + String(fileSize) + ",10000"; // 10 sec timeout
    if (sendATCommand(httpDataCmd.c_str(), 10000, "DOWNLOAD") == "") {
        Serial.println("Failed to prepare for data upload.");
        file.close();
        sendATCommand("AT+HTTPTERM", 10000, "OK");
        return;
    }
    
    Serial.println("Sending file data...");
    // Write file data to modem
    size_t bytesSent = 0;
    unsigned long start_time = millis();
    while (file.available()) {
        char buf[64];
        int toRead = file.available() > 64 ? 64 : file.available();
        int bytesRead = file.read((uint8_t*)buf, toRead);
        bytesSent += simSerial.write(buf, bytesRead);
    }
    file.close();
    Serial.printf("Sent %d bytes in %lu ms\n", bytesSent, millis() - start_time);


    // Wait for the final "OK" after data has been sent
    String response = "";
    start_time = millis();
    while(millis() - start_time < 20000) { // 20 sec timeout for data confirmation
      if(simSerial.available()){
        String line = simSerial.readStringUntil('\n');
        line.trim();
        if(line.length() > 0) {
          Serial.println(line);
          if(line.indexOf("OK") != -1) {
            response = "OK";
            break;
          }
        }
      }
    }

    if (response != "OK") {
        Serial.println("Did not get confirmation after sending data.");
        sendATCommand("AT+HTTPTERM", 10000, "OK");
        return;
    }

    Serial.println("Executing POST request...");
    if (sendATCommand("AT+HTTPACTION=1", 30000, "+HTTPACTION: 1,200") != "") {
        Serial.println("Upload successful!");
        Serial.println("File URL: " + url.replace("?uploadType=media&name=", "?alt=media&token=")); // A guess at the final URL structure
    } else {
        Serial.println("Upload failed.");
    }

    sendATCommand("AT+HTTPTERM", 10000, "OK");
    sendATCommand("AT+NETCLOSE", 10000, "OK");
}
