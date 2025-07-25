#include <SoftwareSerial.h>
#include <SD.h>
#include <SPI.h>

// Essential Pins
const int SIM7600_TX = 17;
const int SIM7600_RX = 16;
const int SD_CS = 5;

// Define the SoftwareSerial for the SIM7600G
SoftwareSerial simSerial(SIM7600_RX, SIM7600_TX);

// APN for your mobile network provider
const char APN[] = "internet"; // Replace with your APN

// --- Function to send AT commands and wait for a specific response ---
String sendATCommand(String cmd, unsigned long timeout, const char* expected_response) {
    String response = "";
    simSerial.println(cmd);
    unsigned long startTime = millis();
    while ((millis() - startTime) < timeout) {
        if (simSerial.available()) {
            char c = simSerial.read();
            response += c;
        }
    }
    Serial.print(">> " + cmd + "\n" + response);
    if (response.indexOf(expected_response) == -1) {
        Serial.println("[ERROR] Timeout or unexpected response.");
        return "";
    }
    return response;
}

// --- Function to Setup SIM7600G ---
bool setupSIM7600() {
    Serial.println("--- Initializing SIM7600G ---");
    simSerial.begin(115200);
    delay(1000);

    if (sendATCommand("AT", 2000, "OK") == "") return false;
    if (sendATCommand("ATE0", 2000, "OK") == "") return false; // Disable command echo
    if (sendATCommand("AT+CPIN?", 5000, "READY") == "") return false;
    if (sendATCommand("AT+CSQ", 5000, "OK") == "") return false;
    if (sendATCommand("AT+CGREG?", 30000, "+CGREG: 0,1") == "" && sendATCommand("AT+CGREG?", 1000, "+CGREG: 0,5") == "") return false;
    if (sendATCommand("AT+COPS?", 10000, "OK") == "") return false;
    
    Serial.println("--- Activating PDP Context ---");
    String cmd = "AT+CGDCONT=1,\"IP\",\"";
    cmd += APN;
    cmd += "\"";
    if (sendATCommand(cmd, 10000, "OK") == "") return false;
    if (sendATCommand("AT+NETOPEN", 20000, "Network opened") == "") return false;
    if (sendATCommand("AT+IPADDR", 10000, "OK") == "") return false;

    Serial.println("SIM7600G Initialized Successfully.");
    return true;
}

// --- Function to upload a file to Firebase Storage ---
bool uploadFile(String filename) {
    Serial.println("--- Starting Upload to Firebase ---");

    File file = SD.open(filename.c_str(), FILE_READ);
    if (!file) {
        Serial.println("Failed to open file for reading.");
        return false;
    }
    size_t fileSize = file.size();
    Serial.println("File: " + filename + ", Size: " + String(fileSize) + " bytes");

    // 1. Initialize HTTP Service
    if (sendATCommand("AT+HTTPINIT", 10000, "OK") == "") {
        file.close();
        return false;
    }

    // 2. Set HTTP Parameters
    String url = "https://firebasestorage.googleapis.com/v0/b/cellular-data-streamer.appspot.com/o/" + filename + "?uploadType=media&name=" + filename;
    if (sendATCommand("AT+HTTPPARA=\"URL\",\"" + url + "\"", 10000, "OK") == "") {
        file.close();
        sendATCommand("AT+HTTPTERM", 5000, "OK");
        return false;
    }
    if (sendATCommand("AT+HTTPPARA=\"CONTENT\",\"audio/wav\"", 10000, "OK") == "") {
        file.close();
        sendATCommand("AT+HTTPTERM", 5000, "OK");
        return false;
    }

    // 3. Set HTTP Data
    if (sendATCommand("AT+HTTPDATA=" + String(fileSize) + ",10000", 10000, "DOWNLOAD") == "") {
        file.close();
        sendATCommand("AT+HTTPTERM", 5000, "OK");
        return false;
    }

    // 4. Send File Data
    Serial.println("Sending file data...");
    byte buffer[256];
    while (file.available()) {
        int bytesRead = file.read(buffer, sizeof(buffer));
        simSerial.write(buffer, bytesRead);
    }
    file.close();
    Serial.println("File data sent.");
    
    // Wait for OK after data transfer
    unsigned long startTime = millis();
    String response = "";
    while(millis() - startTime < 15000) { // 15 sec timeout for data post
        if(simSerial.available()) {
            response += (char)simSerial.read();
            if(response.indexOf("OK") != -1) break;
        }
    }
    Serial.println(">> " + response);
    if(response.indexOf("OK") == -1) {
       Serial.println("Failed to get OK after sending data.");
       sendATCommand("AT+HTTPTERM", 5000, "OK");
       return false;
    }


    // 5. Send POST Request
    if (sendATCommand("AT+HTTPACTION=1", 30000, "+HTTPACTION: 1,200") == "") {
        Serial.println("HTTP POST request failed.");
        sendATCommand("AT+HTTPTERM", 5000, "OK");
        return false;
    }
    
    Serial.println("File uploaded successfully!");
    Serial.println("File URL: https://firebasestorage.googleapis.com/v0/b/cellular-data-streamer.appspot.com/o/" + filename + "?alt=media");


    // 6. Terminate HTTP Service
    sendATCommand("AT+HTTPTERM", 5000, "OK");
    return true;
}

void setup() {
    Serial.begin(115200);
    while (!Serial) {
        ; // wait for serial port to connect.
    }
    Serial.println("--- System Start ---");

    // Initialize SD Card
    Serial.println("Initializing SD card...");
    if (!SD.begin(SD_CS)) {
        Serial.println("SD Card initialization failed!");
        while (1)
            ; // Halt
    }
    Serial.println("SD card initialized.");

    // List WAV files
    File root = SD.open("/");
    if (!root) {
        Serial.println("Failed to open root directory.");
        return;
    }

    String files[20];
    int fileCount = 0;
    while (true) {
        File entry = root.openNextFile();
        if (!entry) {
            break; // No more files
        }
        if (String(entry.name()).endsWith(".wav") && fileCount < 20) {
            files[fileCount++] = String(entry.name());
        }
        entry.close();
    }
    root.close();

    if (fileCount == 0) {
        Serial.println("No .wav files found on SD card.");
        while(1);
    }

    Serial.println("\n--- Available .wav Files ---");
    for (int i = 0; i < fileCount; i++) {
        Serial.println(String(i + 1) + ": " + files[i]);
    }
    
    // User selection
    int choice = -1;
    Serial.print("\nEnter the number of the file to upload: ");
    while (choice < 1 || choice > fileCount) {
        if (Serial.available()) {
            String input = Serial.readStringUntil('\n');
            choice = input.toInt();
            if (choice < 1 || choice > fileCount) {
                 Serial.print("Invalid choice. Please enter a number between 1 and " + String(fileCount) + ": ");
            }
        }
    }
    
    String selectedFile = files[choice - 1];
    Serial.println("You selected: " + selectedFile);


    // Initialize SIM7600G
    if (!setupSIM7600()) {
        Serial.println("HALTED: SIM7600G Error.");
        while (1);
    }

    // Upload the selected file
    if(uploadFile(selectedFile)) {
        Serial.println("--- Process Complete ---");
    } else {
        Serial.println("--- Process Failed ---");
    }
    
    // Cleanup network connection
    sendATCommand("AT+NETCLOSE", 10000, "OK");
}

void loop() {
    // Everything is done in setup, so loop is empty.
    // You could implement logic to enter a low-power mode here.
    delay(10000);
}
