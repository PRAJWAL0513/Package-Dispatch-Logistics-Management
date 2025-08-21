#include <SPI.h>
#include <MFRC522.h>
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>

// -----------------------------------------
// RFID Pins
#define SS_PIN_1   5
#define RST_PIN_1  15
#define SS_PIN_2   21
#define RST_PIN_2  22
#define BUZZER     4

MFRC522 mfrc522_1(SS_PIN_1, RST_PIN_1);
MFRC522 mfrc522_2(SS_PIN_2, RST_PIN_2);

// -----------------------------------------
// WiFi
const char* WIFI_SSID = "ssid";        // Change this
const char* WIFI_PASSWORD = "password"; // Change this

// Google Sheets
const String deployment_id = "https://script.google.com/macros/s/DEPLOYMENT_ID"; 
const String query_suffix = "&name1=bang";

// -----------------------------------------
// Web Server
WebServer server(80);

// Store last 10 scanned cards per location
#define MAX_LOGS 10
String logsA[MAX_LOGS];
String logsB[MAX_LOGS];
int indexA = 0, indexB = 0;

// -----------------------------------------
// Setup
void setup() {
  Serial.begin(115200);
  SPI.begin();

  // Initialize both readers
  mfrc522_1.PCD_Init();
  delay(50);
  mfrc522_2.PCD_Init();
  delay(50);

  pinMode(BUZZER, OUTPUT);
  noTone(BUZZER);

  // WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println("\n✅ WiFi connected!");
  Serial.println(WiFi.localIP());

  // Setup Web Server
  server.on("/", handleRoot);
  server.begin();
  Serial.println("🌍 Web server started");
}

// -----------------------------------------
// Loop
void loop() {
  server.handleClient();

  CheckReader(mfrc522_1, "Reader1", logsA, indexA);
  delay(50);
  CheckReader(mfrc522_2, "Reader2", logsB, indexB);
  delay(200);
}

// -----------------------------------------
// RFID Check Function
void CheckReader(MFRC522 &reader, String readerName, String logs[], int &logIndex) {
  if (!reader.PICC_IsNewCardPresent() || !reader.PICC_ReadCardSerial()) return;

  // Build UID
  String uid = "";
  for (byte i = 0; i < reader.uid.size; i++) {
    uid += String(reader.uid.uidByte[i], HEX);
  }
  uid.toUpperCase();

  Serial.println(readerName + " scanned UID: " + uid);

  BeepSuccess();

  // Save to logs
  logs[logIndex] = uid;
  logIndex = (logIndex + 1) % MAX_LOGS;

  // Send to Google Sheets
  if (WiFi.status() == WL_CONNECTED) {
    SendToGoogleSheets(uid, readerName);
  }

  reader.PICC_HaltA();
  reader.PCD_StopCrypto1();
}

// -----------------------------------------
// Google Sheets Sender
void SendToGoogleSheets(String uid, String readerName) {
  HTTPClient http;
  String url = deployment_id + "/exec?uid=" + uid + "&reader=" + readerName + query_suffix;
  http.begin(url);
  int httpCode = http.GET();
  if (httpCode > 0) {
    Serial.printf("✅ Sheets Response: %d\n", httpCode);
  } else {
    Serial.printf("❌ Sheets Error: %s\n", http.errorToString(httpCode).c_str());
  }
  http.end();
}

// -----------------------------------------
// Web Page Generator
String buildTableRows(String logs[]) {
  String rows = "";
  for (int i = 0; i < MAX_LOGS; i++) {
    if (logs[i].length() > 0) {
      rows += "<tr><td>" + String(i+1) + "</td>";
      rows += "<td>" + logs[i] + "</td>";
      rows += "<td>" + String(__DATE__) + "</td>";
      rows += "<td>" + String(__TIME__) + "</td></tr>";
    }
  }
  return rows;
}

String getHTML() {
  String html = R"rawliteral(
  <!DOCTYPE html>
  <html lang="en">
  <head>
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <style>
  table { border-collapse: collapse; width: 100%; }
  th, td { text-align: left; padding: 8px; }
  tr:nth-child(even) { background-color: #D6EEEE; }
  </style>
  <title>Package Tracking</title>
  </head>
  <body>
    <h1>Package Tracking from Location A to Location B</h1>
    <table>
      <tr>
        <td>
          <table border="2">
            <caption><h1>Location A</h1></caption>
            <tr><th>Sl. No.</th><th>Article (UID)</th><th>Date</th><th>Time</th></tr>
            %LOCATION_A_ROWS%
          </table>
        </td>
        <td>
          <table border="2">
            <caption><h1>Location B</h1></caption>
            <tr><th>Sl. No.</th><th>Article (UID)</th><th>Date</th><th>Time</th></tr>
            %LOCATION_B_ROWS%
          </table>
        </td>
      </tr>
    </table>
  </body>
  </html>
  )rawliteral";

  html.replace("%LOCATION_A_ROWS%", buildTableRows(logsA));
  html.replace("%LOCATION_B_ROWS%", buildTableRows(logsB));
  return html;
}

void handleRoot() {
  server.send(200, "text/html", getHTML());
}

// -----------------------------------------
// Buzzer Functions
void BeepSuccess() {
  tone(BUZZER, 1000, 150);
  delay(200);
  noTone(BUZZER);
}

void BeepError() {
  tone(BUZZER, 800, 500);
  delay(600);
  noTone(BUZZER);
}
