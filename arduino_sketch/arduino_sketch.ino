#include <SPI.h>
#include <SD.h>
#include <HardwareSerial.h>

// Pin definitions
#define SD_CS 5
#define SIM7600_TX 17
#define SIM7600_RX 16

// Network configuration
String apn = "internet"; // Your SIM card's APN

// Server configuration
String host = "cellular-data-streamer.web.app";
String path = "/api/upload";
int port = 443; // Use 443 for HTTPS

// Hardware Serial for SIM7600G
HardwareSerial modem(2);

// --- Function Prototypes ---
void setup();
void loop();
String sendATCommand(const char* cmd, unsigned long timeout, bool check);
void setupSIM7600();
void listFiles();
String selectFile();
void uploadFile(String filename);

// --- Setup ---
void setup() {
    Serial.begin(115200);
    while (!Serial) {
        ; // wait for serial port to connect.
    }
    Serial.println("\n[INFO] Starting up...");

    // Start modem serial
    modem.begin(115200);

    // Initialize SD card
    Serial.println("[INFO] Initializing SD card...");
    if (!SD.begin(SD_CS)) {
        Serial.println("[ERROR] SD Card initialization failed!");
        while (1);
    }
    Serial.println("[INFO] SD Card initialized.");

    // Setup SIM7600G
    setupSIM7600();

    // Main logic
    String filenameToUpload = selectFile();
    if (filenameToUpload != "") {
        uploadFile(filenameToUpload);
    } else {
        Serial.println("[INFO] No file selected for upload.");
    }

    Serial.println("[INFO] Process finished. Going into deep sleep.");
}

// --- Loop (unused) ---
void loop() {
    // Intentionally empty
}

// --- Function Definitions ---

/**
 * Sends an AT command to the modem and waits for a response.
 */
String sendATCommand(const char* cmd, unsigned long timeout, bool check) {
    String response = "";
    unsigned long startTime = millis();

    modem.println(cmd);
    Serial.print(">> ");
    Serial.println(cmd);

    while (millis() - startTime < timeout) {
        if (modem.available()) {
            char c = modem.read();
            response += c;
        }
    }

    Serial.print("<< ");
    Serial.println(response);

    if (check && (response.indexOf("OK") == -1 && response.indexOf("DOWNLOAD") == -1 && response.indexOf(">") == -1)) {
        Serial.println("[ERROR] Timeout or unexpected response.");
        Serial.println("HALTED: SIM7600G Error.");
        while(1); // Halt on error
    }
    return response;
}


/**
 * Initializes the SIM7600G module.
 */
void setupSIM7600() {
    Serial.println("[INFO] Setting up SIM7600G...");
    sendATCommand("AT", 1000, true);
    sendATCommand("ATE0", 1000, true);
    sendATCommand("AT+CPIN?", 5000, true);
    sendATCommand("AT+CSQ", 5000, true);

    Serial.println("[INFO] Waiting for network registration...");
    while (sendATCommand("AT+CGREG?", 1000, false).indexOf("+CGREG: 0,1") == -1) {
        delay(2000);
    }
    Serial.println("[INFO] Registered to network.");

    sendATCommand("AT+COPS?", 5000, true); // Check operator
    String cmd = "AT+CGDCONT=1,\"IP\",\"" + apn + "\"";
    sendATCommand(cmd.c_str(), 5000, true); // Set APN
    sendATCommand("AT+CNMP=2", 3000, true); // Prefer LTE
    sendATCommand("AT+CNACT=1,1", 20000, true); // Activate PDP context
    
    Serial.println("[INFO] SIM7600G setup complete.");
}

/**
 * Lists all files in the root directory of the SD card.
 */
void listFiles() {
    Serial.println("\n[INFO] Files on SD card:");
    File root = SD.open("/");
    if (root) {
        int fileCount = 0;
        while (true) {
            File entry = root.openNextFile();
            if (!entry) {
                // no more files
                break;
            }
            if (!entry.isDirectory()) {
                fileCount++;
                Serial.print(fileCount);
                Serial.print(": ");
                Serial.println(entry.name());
            }
            entry.close();
        }
        root.close();
        if (fileCount == 0) {
            Serial.println("  No files found.");
        }
    } else {
        Serial.println("[ERROR] Could not open root directory.");
    }
}

/**
 * Allows the user to select a file from the SD card.
 */
String selectFile() {
    listFiles();
    Serial.print("\nEnter the number of the file to upload: ");
    while (!Serial.available()) {
        delay(100);
    }

    int choice = Serial.parseInt();
    Serial.println(choice); // Echo choice

    if (choice > 0) {
        File root = SD.open("/");
        int fileCount = 0;
        String filename = "";
        while (true) {
            File entry = root.openNextFile();
            if (!entry) break;

            if (!entry.isDirectory()) {
                fileCount++;
                if (fileCount == choice) {
                    filename = entry.name();
                    break;
                }
            }
            entry.close();
        }
        root.close();
        return filename;
    }
    return "";
}

/**
 * Uploads a file to the server using chunked transfer.
 */
void uploadFile(String filename) {
    File file = SD.open("/" + filename, FILE_READ);
    if (!file) {
        Serial.println("[ERROR] Failed to open file for reading.");
        return;
    }

    long fileSize = file.size();
    Serial.print("[INFO] Uploading ");
    Serial.print(filename);
    Serial.print(" (");
    Serial.print(fileSize);
    Serial.println(" bytes)");

    // Enable SSL for HTTP
    sendATCommand("AT+HTTPSSL=1", 5000, true);
    
    // Initialize HTTP service
    sendATCommand("AT+HTTPINIT", 10000, true);

    // Set HTTP parameters
    String httpUrl = "https://" + host + path;
    sendATCommand(("AT+HTTPPARA=\"URL\",\"" + httpUrl + "\"").c_str(), 5000, true);
    
    const size_t CHUNK_SIZE = 1024; // 1KB chunks
    long totalChunks = (fileSize + CHUNK_SIZE - 1) / CHUNK_SIZE;
    String fileIdentifier = filename + "-" + String(fileSize);

    for (long chunkIndex = 0; chunkIndex < totalChunks; chunkIndex++) {
        long start = chunkIndex * CHUNK_SIZE;
        long end = start + CHUNK_SIZE;
        if (end > fileSize) {
            end = fileSize;
        }
        long currentChunkSize = end - start;

        Serial.print("\n[INFO] Uploading chunk ");
        Serial.print(chunkIndex + 1);
        Serial.print("/");
        Serial.print(totalChunks);
        Serial.print(" (");
        Serial.print(currentChunkSize);
        Serial.println(" bytes)");

        // The boundary is a fixed string that separates parts of the multipart form data.
        String boundary = "----WebKitFormBoundary7MA4YWxkTrZu0gW";
        String contentType = "multipart/form-data; boundary=" + boundary;
        
        // Construct the body of the request
        String data = "--" + boundary + "\r\n";
        data += "Content-Disposition: form-data; name=\"chunk\"; filename=\"" + filename + "\"\r\n";
        data += "Content-Type: application/octet-stream\r\n\r\n";

        String data_end = "\r\n--" + boundary + "\r\n";
        data_end += "Content-Disposition: form-data; name=\"fileIdentifier\"\r\n\r\n" + fileIdentifier + "\r\n";
        data_end += "--" + boundary + "\r\n";
        data_end += "Content-Disposition: form-data; name=\"chunkIndex\"\r\n\r\n" + String(chunkIndex) + "\r\n";
        data_end += "--" + boundary + "\r\n";
        data_end += "Content-Disposition: form-data; name=\"totalChunks\"\r\n\r\n" + String(totalChunks) + "\r\n";
        data_end += "--" + boundary + "\r\n";
        data_end += "Content-Disposition: form-data; name=\"originalFilename\"\r\n\r\n" + filename + "\r\n";
        data_end += "--" + boundary + "--\r\n";

        long totalPayloadSize = data.length() + currentChunkSize + data_end.length();
        
        sendATCommand(("AT+HTTPPARA=\"CONTENT\",\"" + contentType + "\"").c_str(), 5000, true);

        // Set the total size of the HTTP data to be sent
        String httpDataCmd = "AT+HTTPDATA=" + String(totalPayloadSize) + ",120000";
        sendATCommand(httpDataCmd.c_str(), 1000, true);

        // Send the payload parts
        modem.print(data);

        // Send the file chunk
        byte buffer[CHUNK_SIZE];
        file.seek(start);
        size_t bytesRead = file.read(buffer, currentChunkSize);
        modem.write(buffer, bytesRead);

        // Send the end part
        modem.print(data_end);
        
        sendATCommand("", 120000, true); // Wait for OK after data
        
        // Send the POST request
        sendATCommand("AT+HTTPACTION=1", 120000, true);

        // Read the response from the server
        sendATCommand("AT+HTTPREAD", 10000, true);
    }

    file.close();

    // Terminate HTTP service
    sendATCommand("AT+HTTPTERM", 5000, true);

    Serial.println("\n[INFO] Upload complete.");
}
