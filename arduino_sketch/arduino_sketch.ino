
#include <HardwareSerial.h>
#include <SD.h>
#include <SPI.h>

// --- Configuration ---
const char* apn = "vodafone"; // Your APN
const char* gprsUser = "";    // GPRS User, if required
const char* gprsPass = "";    // GPRS Password, if required

// Update this to your development server's public URL
const String server = "cellular-data-streamer.web.app";
const int serverPort = 443;
const String resource = "/api/upload";

// Modem pins
#define MODEM_RX 16
#define MODEM_TX 17
#define MODEM_BAUD 115200

// SD card pins
#define SD_CS 5
#define SD_MOSI 23
#define SD_MISO 19
#define SD_SCK 18

// --- Globals ---
HardwareSerial modemSerial(1);
const int CHUNK_SIZE = 4096; // 4 KB chunks
char data[CHUNK_SIZE];

// --- Helper Functions ---

/**
 * @brief Sends an AT command and waits for a specific response.
 * @param command The AT command to send.
 * @param expectedResponse The response to wait for.
 * @param timeout The timeout in milliseconds.
 * @return True if the expected response is received, false otherwise.
 */
bool sendAT(const String& command, const String& expectedResponse, unsigned long timeout) {
    String response = "";
    unsigned long startTime = millis();
    
    Serial.print("[AT SEND] ");
    Serial.println(command);

    modemSerial.println(command);

    while (millis() - startTime < timeout) {
        if (modemSerial.available()) {
            char c = modemSerial.read();
            response += c;
            if (response.indexOf(expectedResponse) != -1) {
                Serial.print("[AT RECV] ");
                Serial.println(response);
                return true;
            }
        }
    }

    Serial.println("[DEBUG] Timeout waiting for: " + expectedResponse);
    Serial.println("[DEBUG] Received: " + response);
    return false;
}


/**
 * @brief Configures the modem's SSL context for HTTPS.
 * Sets SSL version and disables strict certificate authentication.
 * @return True on success, false on failure.
 */
bool configureSSL() {
    Serial.println("? Configuring SSL...");

    // Set SSL context to support TLS 1.2 (3)
    if (!sendAT("AT+CSSLCFG=\"sslversion\",1,3", "OK", 5000)) return false;

    // Set authentication mode to 0 (no server certificate verification) for development
    if (!sendAT("AT+CSSLCFG=\"authmode\",1,0", "OK", 5000)) return false;
    
    Serial.println("? SSL Configured.");
    return true;
}


/**
 * @brief Opens a secure HTTPS session with the server.
 * @return True if the session is opened, false otherwise.
 */
bool openHttpsSession() {
    if (!sendAT("AT+CHTTPSSTART", "+CHTTPSSTART: 0", 10000)) {
        // Fallback for some firmware versions
        if (!sendAT("AT+CHTTPSSTART", "OK", 10000)) return false;
    }

    String opseCommand = "AT+CHTTPSOPSE=\"" + server + "\"," + serverPort;
    if (!sendAT(opseCommand, "+CHTTPSOPSE: 0", 15000)) {
        // Fallback for some firmware versions
        if (!sendAT(opseCommand, "OK", 15000)) {
            Serial.println("? Failed to open HTTPS session with server.");
            return false;
        }
    }
    return true;
}

/**
 * @brief Closes the current HTTPS session.
 */
void closeHttpsSession() {
    sendAT("AT+CHTTPSCLSE", "+CHTTPSCLSE: 0", 5000);
    sendAT("AT+CHTTPSSTOP", "+CHTTPSSTOP: 0", 5000);
}

/**
 * @brief Sends a single file chunk via HTTPS POST using AT commands.
 * @param file The file object to read from.
 * @param filename The name of the file being uploaded.
 * @param offset The current position/offset in the file.
 * @param totalSize The total size of the file.
 * @return True if the chunk was sent and acknowledged, false otherwise.
 */
bool sendChunk(File& file, const char* filename, size_t offset, size_t totalSize) {
    size_t chunkSize = file.read(data, CHUNK_SIZE);
    if (chunkSize == 0) return false; // No more data to read

    // --- Construct Headers ---
    String headers = "X-Filename: " + String(filename) + "\r\n" +
                     "X-Chunk-Offset: " + String(offset) + "\r\n" +
                     "X-Chunk-Size: " + String(chunkSize) + "\r\n" +
                     "X-Total-Size: " + String(totalSize) + "\r\n" +
                     "Content-Type: application/octet-stream\r\n";

    int headersLength = headers.length();
    
    // --- Construct AT Command ---
    // AT+CHTTPSPOST=<url>,<header_len>,<content_len>,<content_timeout>
    String postCommand = "AT+CHTTPSPOST=\"" + resource + "\"," + headersLength + "," + chunkSize + ",10000";

    // --- Send Command and Wait for Prompt ---
    if (!sendAT(postCommand, ">", 10000)) {
        Serial.println("? Modem did not respond to POST command. Aborting.");
        return false;
    }

    // --- Send Headers ---
    Serial.print("[HTTP HEADERS] ");
    Serial.print(headers);
    modemSerial.print(headers);

    // --- Send Data Chunk ---
    Serial.printf("[HTTP BODY] Sending %d bytes...\n", chunkSize);
    modemSerial.write((uint8_t*)data, chunkSize);
    
    // --- Wait for Server Response ---
    // A successful response should contain "+CHTTPS: POST,2" (e.g., 200, 201)
    return sendAT("+CHTTPS: POST,2", "OK", 20000);
}


/**
 * @brief Reads a file from the SD card and uploads it in chunks.
 * @param filename The name of the file to upload.
 */
void sendFileChunks(const char* filename) {
    File file = SD.open(filename, FILE_READ);
    if (!file) {
        Serial.println("? Failed to open file for reading.");
        return;
    }

    size_t fileSize = file.size();
    Serial.printf("? Preparing to upload %s (%d bytes)\n", filename, fileSize);

    if (!openHttpsSession()) {
        Serial.println("? Initial HTTPS session failed. Aborting.");
        file.close();
        return;
    }

    while (file.available()) {
        size_t offset = file.position();
        if (!sendChunk(file, filename, offset, fileSize)) {
            Serial.printf("? Failed to upload chunk at offset %d. Retrying session...\n", offset);
            
            // Close the failed session and try opening a new one
            closeHttpsSession();
            if (!openHttpsSession()) {
                 Serial.println("? Could not re-establish HTTPS session. Aborting.");
                 break; 
            }
            
            // Rewind file pointer to retry the same chunk
            file.seek(offset);

            // Retry sending the chunk with the new session
            if (!sendChunk(file, filename, offset, fileSize)) {
                 Serial.printf("? Chunk upload failed again at offset %d. Aborting.\n", offset);
                 break;
            }
        }
    }

    closeHttpsSession();
    file.close();
    Serial.println("? File upload finished.");
}


// --- Main Setup and Loop ---

void setup() {
    Serial.begin(115200);
    while (!Serial);

    Serial.println("? Booting...");

    // Initialize SD Card
    SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
    if (!SD.begin(SD_CS)) {
        Serial.println("? SD Card initialization failed!");
        while (1);
    }
    Serial.println("? SD card ready.");

    // Initialize Modem
    modemSerial.begin(MODEM_BAUD, SERIAL_8N1, MODEM_TX, MODEM_RX);
    Serial.println("Initializing modem...");

    if (!sendAT("AT", "OK", 1000)) {
        Serial.println("? Modem not responding.");
        while(1);
    }
    Serial.println("? Modem restarted.");

    // Print Modem Status
    Serial.println("--- Modem Status ---");
    sendAT("ATI", "OK", 1000);
    sendAT("AT+CSQ", "OK", 1000);
    sendAT("AT+CPIN?", "READY", 5000);
    sendAT("AT+CCID", "OK", 1000);
    sendAT("AT+COPS?", "OK", 5000);
    Serial.println("--------------------");

    // Configure SSL
    if (!configureSSL()) {
        Serial.println("? Failed to configure SSL context.");
        // We can still try to continue, some modems might not need this
    }

    // Connect to GPRS
    Serial.println("? Connecting to network...");
    if (!sendAT("AT+CGATT=1", "OK", 10000)) {
        Serial.println("? Failed to attach to GPRS.");
        while(1);
    }
    
    // Set APN
    sendAT("AT+CSTT=\"" + String(apn) + "\",\"" + String(gprsUser) + "\",\"" + String(gprsPass) + "\"", "OK", 10000);

    // Bring up wireless connection
    sendAT("AT+CIICR", "OK", 20000);
    
    Serial.println("? GPRS connected.");

    // Get Local IP
    sendAT("AT+IPADDR", "+IPADDR", 10000);

    // Start upload
    sendFileChunks("/sigma2.wav");
}

void loop() {
    // Keep modem serial communication lines open
    if (modemSerial.available()) {
        Serial.write(modemSerial.read());
    }
    if (Serial.available()) {
        modemSerial.write(Serial.read());
    }
}
