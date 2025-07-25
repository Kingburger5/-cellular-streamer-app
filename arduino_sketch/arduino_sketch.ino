#include <SPI.h>
#include <SD.h>
#include <HardwareSerial.h>

// Pin definitions
#define SD_CS 5 // SD card chip select pin

// Serial communication with SIM7600G
#define SIM7600_TX 27
#define SIM7600_RX 26
HardwareSerial modem(2);

// Your GPRS credentials
String apn = "internet"; // Set your APN here, e.g., "internet"

// Firebase Storage details
String firebaseBucket = "cellular-data-streamer.appspot.com";

// Function prototypes
bool setupSIM7600();
void listWavFiles();
String getFileChoice();
void uploadFile(String filename);
bool sendATCommand(String cmd, String ack, uint32_t timeout);

void setup() {
    Serial.begin(115200);
    while (!Serial);

    Serial.println("--- ESP32 SIM7600G Firebase Uploader ---");

    // Initialize SD Card
    if (!SD.begin(SD_CS)) {
        Serial.println("SD Card Mount Failed. HALTED.");
        while (true);
    }
    Serial.println("SD Card Initialized.");

    // List .wav files and get user choice
    listWavFiles();
    String filename = getFileChoice();
    if (filename == "") {
        Serial.println("No file selected or invalid input. HALTED.");
        while(true);
    }

    Serial.println("Initializing SIM7600G...");
    modem.begin(115200, SERIAL_8N1, SIM7600_RX, SIM7600_TX);

    if (setupSIM7600()) {
        Serial.println("SIM7600G Initialized Successfully.");
        uploadFile(filename);
    } else {
        Serial.println("HALTED: SIM7600G Initialization Failed.");
        while (true);
    }

    Serial.println("--- Task Complete ---");
}

void loop() {
    // Everything is done in setup
}

void listWavFiles() {
    Serial.println("\nAvailable .wav files on SD Card:");
    File root = SD.open("/");
    int count = 0;
    while (true) {
        File entry = root.openNextFile();
        if (!entry) {
            break; // No more files
        }
        if (String(entry.name()).endsWith(".wav")) {
            count++;
            Serial.print(count);
            Serial.print(": ");
            Serial.println(entry.name());
        }
        entry.close();
    }
    if (count == 0) {
        Serial.println("No .wav files found.");
    }
    root.close();
}

String getFileChoice() {
    Serial.print("\nEnter the number of the file to upload: ");
    String fileNumberStr = "";
    while (true) {
        if (Serial.available()) {
            char c = Serial.read();
            if (c == '\n' || c == '\r') {
                if (fileNumberStr.length() > 0) break;
            } else {
                fileNumberStr += c;
                Serial.print(c);
            }
        }
    }
    
    int fileNumber = fileNumberStr.toInt();
    if (fileNumber <= 0) return "";

    File root = SD.open("/");
    int count = 0;
    String filename = "";
    while (true) {
        File entry = root.openNextFile();
        if (!entry) {
            break;
        }
        if (String(entry.name()).endsWith(".wav")) {
            count++;
            if (count == fileNumber) {
                filename = entry.name();
                entry.close();
                break;
            }
        }
        entry.close();
    }
    root.close();
    
    // The filename might include a leading slash, remove it.
    if (filename.startsWith("/")) {
        filename = filename.substring(1);
    }
    
    return filename;
}


bool sendATCommand(String cmd, String ack, uint32_t timeout) {
    // Print the command being sent
    Serial.print(">> ");
    Serial.println(cmd);
    
    // Send the command to the modem
    modem.println(cmd);

    // Read response
    String response = "";
    uint32_t startTime = millis();
    while (millis() - startTime < timeout) {
        if (modem.available()) {
            char c = modem.read();
            response += c;
        }
        // Check for acknowledgment string or ERROR
        if (response.indexOf(ack) != -1) {
            Serial.print(response);
            return true;
        }
        if (response.indexOf("ERROR") != -1) {
            Serial.print(response);
            return false;
        }
    }
    
    // If we reach here, it's a timeout
    Serial.println("[ERROR] Timeout or unexpected response.");
    Serial.print("Last response: ");
    Serial.println(response);
    return false;
}

bool setupSIM7600() {
    Serial.println("--- SIM7600G CONNECTION PHASE ---");
    
    if (!sendATCommand("AT", "OK", 5000)) return false;
    if (!sendATCommand("ATE0", "OK", 5000)) return false; // Disable echo
    if (!sendATCommand("AT+CPIN?", "READY", 5000)) return false;
    
    Serial.println("[INFO] Checking signal quality...");
    if (!sendATCommand("AT+CSQ", "OK", 5000)) return false;

    Serial.println("[INFO] Checking network registration...");
    // Wait up to 30 seconds for network registration
    unsigned long startTime = millis();
    bool registered = false;
    while(millis() - startTime < 30000) {
        if(sendATCommand("AT+CGREG?", "+CGREG: 0,1", 2000) || sendATCommand("AT+CGREG?", "+CGREG: 0,5", 2000)){
            registered = true;
            break;
        }
        delay(2000);
    }

    if (!registered) {
        Serial.println("[ERROR] Not registered to network.");
        return false;
    }
    Serial.println("[INFO] Registered to network.");

    if (!sendATCommand("AT+COPS?", "OK", 10000)) return false;
    
    // Activate PDP Context (Mobile Data)
    String cmd = "AT+CGDCONT=1,\"IP\",\"" + apn + "\"";
    if (!sendATCommand(cmd, "OK", 10000)) {
        return false;
    }

    if (!sendATCommand("AT+NETOPEN", "OK", 10000)) return false;
    if (!sendATCommand("AT+IPADDR", "OK", 10000)) return false;
    
    return true;
}

void uploadFile(String filename) {
    Serial.println("\n--- UPLOAD TO FIREBASE ---");

    File file = SD.open("/" + filename, FILE_READ);
    if (!file) {
        Serial.println("Failed to open file for reading.");
        return;
    }
    long fileSize = file.size();
    Serial.print("Uploading file: ");
    Serial.print(filename);
    Serial.print(" (");
    Serial.print(fileSize);
    Serial.println(" bytes)");

    // 1. Initialize HTTP Service
    if (!sendATCommand("AT+HTTPINIT", "OK", 10000)) {
        file.close();
        return;
    }

    // 2. Set HTTP Parameters
    String url = "https://firebasestorage.googleapis.com/v0/b/" + firebaseBucket + "/o/" + filename + "?uploadType=media&name=" + filename;
    if (!sendATCommand("AT+HTTPPARA=\"URL\",\"" + url + "\"", "OK", 10000)) {
        sendATCommand("AT+HTTPTERM", "OK", 5000);
        file.close();
        return;
    }
    if (!sendATCommand("AT+HTTPPARA=\"CONTENT\",\"audio/wav\"", "OK", 10000)) {
        sendATCommand("AT+HTTPTERM", "OK", 5000);
        file.close();
        return;
    }

    // 3. Set HTTP Data
    if (!sendATCommand("AT+HTTPDATA=" + String(fileSize) + ",120000", "DOWNLOAD", 10000)) {
        sendATCommand("AT+HTTPTERM", "OK", 5000);
        file.close();
        return;
    }

    // 4. Send File Data
    Serial.println("Sending file data...");
    uint8_t buffer[256];
    size_t bytesRead = 0;
    while (bytesRead < fileSize) {
        size_t toRead = (fileSize - bytesRead) > sizeof(buffer) ? sizeof(buffer) : (fileSize - bytesRead);
        size_t didRead = file.read(buffer, toRead);
        if (didRead == 0) break;
        modem.write(buffer, didRead);
        bytesRead += didRead;
        Serial.print(".");
    }
    Serial.println("\nFile data sent.");
    file.close();
    
    // Wait for the OK after data sending
    String response = "";
    unsigned long startTime = millis();
    while (millis() - startTime < 120000) { // Long timeout for upload
      if (modem.available()) {
        char c = modem.read();
        response += c;
        if (response.endsWith("OK")) {
          Serial.println("Modem confirmed data reception.");
          break;
        }
      }
    }


    // 5. Send POST Request
    if (!sendATCommand("AT+HTTPACTION=1", "+HTTPACTION: 1,200", 120000)) {
        Serial.println("POST request failed.");
    } else {
        Serial.println("File uploaded successfully!");
        Serial.print("URL: ");
        Serial.println(url);
    }

    // 6. Terminate HTTP session
    sendATCommand("AT+HTTPTERM", "OK", 5000);
    sendATCommand("AT+NETCLOSE", "OK", 5000);

    Serial.println("--- Cleanup Complete ---");
}
