
// Define the serial port for debugging.
#define SerialMon Serial
// Define the serial port for the SIM7600 module.
#define SerialAT Serial1

// --- Pin Definitions ---
// The GPIO pin connected to the SIM7600's TXD pin
#define PIN_TX 27
// The GPIO pin connected to the SIM7600's RXD pin
#define PIN_RX 26
// The Chip Select pin for the SD card
#define SD_CS 5

// --- Network Configuration ---
// Your GPRS APN (Access Point Name)
const char apn[] = "internet";
// GPRS username (if required)
const char gprsUser[] = "";
// GPRS password (if required)
const char gprsPass[] = "";

// --- Server Configuration ---
// The hostname of your web server
const char server[] = "6000-firebase-studio-1753223410587.cluster-73qgvk7hjjadkrjeyexca5ivva.cloudworkstations.dev";
// The path to your upload API endpoint
const char resource[] = "/api/upload";
// The server port (443 for HTTPS)
const int port = 443;

// --- File Upload Configuration ---
// The size of each chunk to upload, in bytes. 4KB is a good balance.
const size_t CHUNK_SIZE = 4096;
// The number of times to retry uploading a chunk before giving up.
const int UPLOAD_ATTEMPTS = 3;


// --- Library Includes ---
#define TINY_GSM_MODEM_SIM7600
#define TINY_GSM_DEBUG SerialMon
#include <TinyGsmClient.h>
#include <SPI.h>
#include <SD.h>
#include <FS.h>

// --- Global Objects ---
// TinyGSM library client for secure connections
TinyGsmClientSecure client(SerialAT);
// File object for the file being uploaded
File uploadFile;

// --- Main Sketch ---

void setup() {
    // Start serial communication for debugging
    SerialMon.begin(115200);
    delay(10);

    SerialMon.println(F("? Booting..."));

    // Initialize the SD card
    if (!SD.begin(SD_CS)) {
        SerialMon.println(F("! SD card initialization failed."));
        while (1);
    }
    SerialMon.println(F("? SD card ready."));

    // Set up the serial connection to the SIM7600
    SerialAT.begin(115200, SERIAL_8N1, PIN_RX, PIN_TX);

    // Initialize the modem
    SerialMon.println(F("? Initializing modem..."));
    if (!modem.init()) {
        SerialMon.println(F("! Failed to initialize modem. Check connections."));
        while (1);
    }
    SerialMon.println(F("? Waiting for modem to be ready..."));
    modem.waitAndClear(30000L);
    SerialMon.println(F("? Modem is ready."));

    // Print modem information for diagnostics
    printModemInfo();

    // Connect to the network
    if (!connectToNetwork()) {
        while (1); // Halt on failure
    }
}

void loop() {
    SerialMon.println(F("? Starting file listing..."));
    File root = SD.open("/");
    printDirectory(root, 0);
    root.close();
    SerialMon.println(F("? File listing complete."));

    // Attempt to upload the first file found.
    // In a real application, you might choose a specific file.
    root = SD.open("/");
    File fileToUpload = root.openNextFile();
    root.close();

    if (fileToUpload) {
        String filename = "/" + String(fileToUpload.name());
        uploadFile(filename.c_str());
        fileToUpload.close();
    } else {
        SerialMon.println(F("? No files to upload."));
    }

    // Wait for a long time before trying again.
    SerialMon.println(F("? Entering deep sleep for 1 hour."));
    delay(3600000);
}


// --- Helper Functions ---

void printModemInfo() {
    SerialMon.println(F("--- Modem Status ---"));
    SerialMon.println(F("IMEI: ") + modem.getIMEI());
    SerialMon.println(F("Signal Quality: ") + String(modem.getSignalQuality()));
    SerialMon.println(F("SIM Status: ") + String((int)modem.getSimStatus()));
    SerialMon.println(F("CCID: ") + modem.getSimCCID());
    SerialMon.println(F("Operator: ") + modem.getOperator());
    SerialMon.println(F("--------------------"));
}

bool connectToNetwork() {
    SerialMon.println(F("? Waiting for network registration..."));
    if (!modem.waitForNetwork(300000L)) { // 5-minute timeout
        SerialMon.println(F("! Network registration failed."));
        return false;
    }
    SerialMon.println(F("? Registered on network."));

    SerialMon.println(F("? Connecting to GPRS..."));
    if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
        SerialMon.println(F("! GPRS connection failed."));
        return false;
    }

    String ip = modem.getLocalIP();
    SerialMon.println(F("? GPRS Connected. IP: ") + ip);
    if (ip == "0.0.0.0") {
        SerialMon.println(F("! Failed to get a valid IP address."));
        return false;
    }

    return true;
}

// Uploads a file to the server in chunks.
bool uploadFile(const char* filename) {
    // Open the file for reading.
    uploadFile = SD.open(filename, FILE_READ);
    if (!uploadFile) {
        SerialMon.println("! Failed to open file for reading: " + String(filename));
        return false;
    }

    size_t fileSize = uploadFile.size();
    SerialMon.println("? Preparing to upload " + String(filename) + " (" + String(fileSize) + " bytes)");

    // Loop through the file and send it in chunks.
    for (size_t offset = 0; offset < fileSize; offset += CHUNK_SIZE) {
        bool chunkSent = false;
        // Retry logic for each chunk.
        for (int attempt = 1; attempt <= UPLOAD_ATTEMPTS; attempt++) {
            SerialMon.println("  Attempt " + String(attempt) + "/" + String(UPLOAD_ATTEMPTS) + " to send chunk at offset " + String(offset) + "...");

            // Establish a new secure connection for each chunk attempt.
            TinyGsmClientSecure client(SerialAT);
            if (!client.connect(server, port)) {
                SerialMon.println("! Connection failed on attempt " + String(attempt));
                delay(2000); // Wait before retrying
                continue;
            }
            SerialMon.println("? Connected to server.");

            // Determine the size of the current chunk.
            size_t currentChunkSize = min((size_t)CHUNK_SIZE, fileSize - offset);

            // --- Send HTTP Headers ---
            client.println("POST " + String(resource) + " HTTP/1.1");
            client.println("Host: " + String(server));
            // Custom headers for the server to reassemble the file.
            client.println("X-Filename: " + String(uploadFile.name()));
            client.println("X-Chunk-Offset: " + String(offset));
            client.println("X-Chunk-Size: " + String(currentChunkSize));
            client.println("X-Total-Size: " + String(fileSize));
            client.println("Content-Type: application/octet-stream");
            client.println("Content-Length: " + String(currentChunkSize));
            client.println("Connection: close");
            client.println(); // End of headers

            // --- Send Binary Chunk Data ---
            // Move the file pointer to the correct offset.
            uploadFile.seek(offset);
            // Create a buffer to hold the chunk data.
            byte buffer[CHUNK_SIZE];
            // Read the chunk from the SD card into the buffer.
            size_t bytesRead = uploadFile.read(buffer, currentChunkSize);

            // Write the buffer to the client stream.
            if (bytesRead > 0) {
                client.write(buffer, bytesRead);
            }

            SerialMon.println("? Chunk sent, waiting for response...");

            // --- Wait for and process the server response ---
            unsigned long timeout = millis();
            while (client.connected() && millis() - timeout < 15000L) {
                if (client.available()) {
                    String line = client.readStringUntil('\n');
                    // Check for a successful HTTP status code.
                    if (line.startsWith("HTTP/1.1 200 OK")) {
                        chunkSent = true;
                    }
                    SerialMon.println(line);
                }
            }

            // Clean up the connection.
            client.stop();
            SerialMon.println("? Connection closed.");

            if (chunkSent) {
                SerialMon.println("? Chunk at offset " + String(offset) + " uploaded successfully.");
                break; // Exit the retry loop and move to the next chunk.
            } else {
                 SerialMon.println("! Chunk upload failed on attempt " + String(attempt));
            }
        } // End of retry loop

        if (!chunkSent) {
            SerialMon.println("! Failed to upload chunk at offset " + String(offset) + " after " + String(UPLOAD_ATTEMPTS) + " attempts. Aborting upload.");
            uploadFile.close();
            return false;
        }
    } // End of chunk loop

    SerialMon.println("? File upload completed successfully.");
    uploadFile.close();
    return true;
}


// A utility function to print the contents of a directory.
void printDirectory(File dir, int numTabs) {
    while (true) {
        File entry = dir.openNextFile();
        if (!entry) {
            // no more files
            break;
        }
        for (uint8_t i = 0; i < numTabs; i++) {
            SerialMon.print('\t');
        }
        SerialMon.print(entry.name());
        if (entry.isDirectory()) {
            SerialMon.println("/");
            printDirectory(entry, numTabs + 1);
        } else {
            // files have sizes, directories do not
            SerialMon.print("\t\t");
            SerialMon.println(entry.size(), DEC);
        }
        entry.close();
    }
}
