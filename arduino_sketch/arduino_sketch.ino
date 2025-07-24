// Define the serial port for communication with the SIM7600 module
#define DUMP_AT_COMMANDS
#define TINY_GSM_MODEM_SIM7600
#define SerialMon Serial
#define SerialAT Serial1

// Pin definitions for the SIM7600 module
#define UART_BAUD           115200
#define PIN_DTR             25
#define PIN_TX              26
#define PIN_RX              27
#define PIN_PWRKEY          4
#define PIN_RST             5
#define PIN_FLIGHT          23
#define PIN_RI              34

// SD Card pins
#define SD_MISO             2
#define SD_MOSI             15
#define SD_SCLK             14
#define SD_CS               13

#include <SPI.h>
#include <SD.h>
#include <Update.h>

const char apn[]  = "TM";
const char user[] = "";
const char pass[] = "";

// Firebase Storage details
const char server[] = "firebasestorage.googleapis.com";
const int  port = 443;
const char storageBucket[] = "cellular-data-streamer.firebasestorage.app";

File file;

// Function to send AT commands and wait for a response
String sendATCommand(const char* cmd, unsigned long timeout, const char* expected_response) {
  String response = "";
  SerialAT.println(cmd);
  SerialMon.print(">> ");
  SerialMon.println(cmd);

  unsigned long startTime = millis();
  while (millis() - startTime < timeout) {
    if (SerialAT.available()) {
      char c = SerialAT.read();
      response += c;
    }
    if (expected_response != NULL && response.indexOf(expected_response) != -1) {
      break;
    }
  }
  SerialMon.print("<< ");
  SerialMon.println(response);
  return response;
}

void modemPowerOn() {
  pinMode(PIN_PWRKEY, OUTPUT);
  digitalWrite(PIN_PWRKEY, LOW);
  delay(100);
  digitalWrite(PIN_PWRKEY, HIGH);
  delay(1000);
  digitalWrite(PIN_PWRKEY, LOW);
}

void setup() {
  SerialMon.begin(115200);
  delay(10);

  SerialMon.println("🚀 Initializing...");

  // Set up modem serial communication
  SerialAT.begin(UART_BAUD, SERIAL_8N1, PIN_RX, PIN_TX);

  // Power on the modem
  modemPowerOn();
  
  // Wait for the modem to be ready
  delay(5000);
  
  sendATCommand("ATE0", 1000, "OK"); // Echo off
  sendATCommand("AT", 1000, "OK");
  sendATCommand("AT+CPIN?", 5000, "READY");
  sendATCommand("AT+CNMP=38", 3000, "OK"); // 4G/LTE Only
  sendATCommand("AT+CSQ", 1000, "OK");
  sendATCommand("AT+CGDCONT=1,\"IP\",\"" APN "\"", 3000, "OK");
  sendATCommand("AT+CGACT=1,1", 3000, "OK");

  // Initialize SD card
  SPI.begin(SD_SCLK, SD_MISO, SD_MOSI, SD_CS);
  if (!SD.begin(SD_CS)) {
    SerialMon.println("❌ SD Card initialization failed!");
    while (1);
  }
  SerialMon.println("✅ SD Card initialized.");

  // List files on SD card
  File root = SD.open("/");
  while (true) {
    File entry = root.openNextFile();
    if (!entry) {
      break;
    }
    SerialMon.println(entry.name());
    entry.close();
  }
  root.close();

  // The file to upload
  const char* filename = "/sigma1.wav";
  uploadFile(filename);
}

void loop() {
  // Everything is done in setup for this example
}

void uploadFile(const char* filename) {
  file = SD.open(filename, FILE_READ);
  if (!file) {
    SerialMon.println("❌ Failed to open file for reading.");
    return;
  }
  
  long fileSize = file.size();
  SerialMon.print("📄 File: ");
  SerialMon.print(filename);
  SerialMon.print(", Size: ");
  SerialMon.println(fileSize);

  // Enable HTTPS
  sendATCommand("AT+CHTTPSSTART", 5000, "OK");
  
  // Construct the URL
  String url = "https://";
  url += server;
  url += "/v0/b/";
  url += storageBucket;
  url += "/o";
  url += filename;
  url += "?uploadType=media&name=";
  url += &filename[1]; // Remove leading '/' from filename for the name parameter
  
  String cmdOpen = "AT+CHTTPSOPSE=\"";
  cmdOpen += url;
  cmdOpen += "\",";
  cmdOpen += "443";
  sendATCommand(cmdOpen.c_str(), 10000, "OK");
  
  // Prepare POST request
  String cmdPost = "AT+CHTTPSPOST=";
  cmdPost += fileSize;
  cmdPost += ",60000,2"; // Timeout, Content-Type format (2=custom)
  sendATCommand(cmdPost.c_str(), 5000, ">");

  // Send headers
  String headers = "Content-Type: audio/wav\r\n";
  SerialAT.print(headers);
  sendATCommand("", 5000, "OK"); // Wait for OK after headers

  // Wait for the final ">" to start sending data
  unsigned long start = millis();
  while (millis() - start < 10000) {
    if (SerialAT.find(">")) {
      SerialMon.println("✅ Ready to send data...");
      break;
    }
  }

  // Send the file data
  byte buffer[256];
  size_t bytesRead = 0;
  while ((bytesRead = file.read(buffer, sizeof(buffer))) > 0) {
    SerialAT.write(buffer, bytesRead);
  }
  file.close();

  // Wait for upload confirmation
  String response = sendATCommand("", 60000, "+CHTTPS: 0"); // Wait for POST to finish
  if (response.indexOf("+CHTTPS: 0") != -1) {
    SerialMon.println("✅ Upload successful.");
  } else {
    SerialMon.println("❌ Upload failed.");
  }

  // Close HTTPS session
  sendATCommand("AT+CHTTPSCLSE", 5000, "OK");
  sendATCommand("AT+CHTTPSSTOP", 5000, "OK");
}
