#include <SPI.h>
#include <MFRC522.h>
#include <WiFi.h>
#include <HTTPClient.h>

//-----------------------------------------
// ESP32 Pin Definitions
#define SS_PIN_1   5    // RFID1 SDA/SS
#define RST_PIN_1  15   // RFID1 RST
#define SS_PIN_2   21   // RFID2 SDA/SS
#define RST_PIN_2  22   // RFID2 RST
#define BUZZER     4    // Buzzer

//-----------------------------------------
MFRC522 mfrc522_1(SS_PIN_1, RST_PIN_1);
MFRC522 mfrc522_2(SS_PIN_2, RST_PIN_2);

//-----------------------------------------
/* Block number to read */
int blockNum = 2;

//-----------------------------------------
// Google Sheets Integration
const String deployment_id = "https://script.google.com/macros/s/AKfycbwEvtBitue--b_KG_TtvDqGtw1HdNqH7EIKoEsEXvKe-Q51o2yyLu6Pi7X0MAKWpSdKWA"; // Your deployment URL
const String query_suffix = "&name1=bang";  // Optional extra data

//-----------------------------------------
// WiFi Credentials
const char* WIFI_SSID = "ssid";        // Replace with your WiFi name
const char* WIFI_PASSWORD = "password";     // Replace with your WiFi password

/****************************************************************************************************
 * setup() function
 ****************************************************************************************************/
void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  Serial.println("\n\n--- Dual RFID to Google Sheets ---");
  Serial.println("Initializing...");

  // Initialize Buzzer
  pinMode(BUZZER, OUTPUT);
  digitalWrite(BUZZER, LOW);

  // Initialize SPI
  SPI.begin(); // ESP32 default: SCK=18, MISO=19, MOSI=23

  // Initialize both RFID readers
  mfrc522_1.PCD_Init();
  delay(50);
  mfrc522_2.PCD_Init();
  delay(50);
  Serial.println("✅ Both RFID readers initialized.");


  byte v1 = mfrc522_1.PCD_ReadRegister(mfrc522_1.VersionReg);
  Serial.print("Reader1 Version: ");
  Serial.println(v1, HEX);

  byte v2 = mfrc522_2.PCD_ReadRegister(mfrc522_2.VersionReg);
  Serial.print("Reader2 Version: ");
  Serial.println(v2, HEX);



  // Connect WiFi
  Serial.print("Connecting to WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ WiFi connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

/****************************************************************************************************
 * loop() function
 ****************************************************************************************************/
void loop() {
  // Check both readers
  CheckReader(mfrc522_1, blockNum, "Reader1");
  delay(50);
  CheckReader(mfrc522_2, blockNum, "Reader2");

  delay(200); // Small delay to reduce CPU usage
}

/****************************************************************************************************
 * CheckReader()
 ****************************************************************************************************/
void CheckReader(MFRC522 &reader, int blockNum, String readerName) {
  if (!reader.PICC_IsNewCardPresent() || !reader.PICC_ReadCardSerial()) {
    return;
  }

  Serial.println("------------------------------------------------");
  Serial.println("📡 Card detected on " + readerName);

  byte readBlockData[18];
  if (!ReadDataFromBlock(reader, blockNum, readBlockData)) {
    BeepError();
    return;
  }

  // Convert block data to string
  String cardData = "";
  for (int i = 0; i < 16; i++) {
    if (readBlockData[i] != 0 && isPrintable(readBlockData[i])) {
      cardData += (char)readBlockData[i];
    }
  }
  cardData.trim();

  if (cardData.length() == 0) {
    Serial.println("❌ No valid data read from RFID card.");
    BeepError();
    return;
  }

  Serial.print("✅ Card Data: ");
  Serial.println(cardData);

  BeepSuccess();

  if (WiFi.status() == WL_CONNECTED) {
    SendToGoogleSheets(cardData, readerName);
  } else {
    Serial.println("❌ WiFi not connected.");
    BeepError();
  }

  reader.PICC_HaltA();
  reader.PCD_StopCrypto1();
}

/****************************************************************************************************
 * SendToGoogleSheets()
 ****************************************************************************************************/
void SendToGoogleSheets(String cardId, String readerName) {
  HTTPClient https;
  String url = deployment_id + "/exec?name=" + cardId + "&reader=" + readerName + query_suffix;

  Serial.print("📡 Sending request: ");
  Serial.println(url);

  https.begin(url);
  int httpCode = https.GET();

  if (httpCode > 0) {
    Serial.printf("✅ HTTP %d: Data sent successfully!\n", httpCode);
    String response = https.getString();
    Serial.println("Response: " + response);
  } else {
    Serial.printf("❌ HTTP Failed, error: %s\n", https.errorToString(httpCode).c_str());
  }

  https.end();
  delay(500);
}

/****************************************************************************************************
 * ReadDataFromBlock()
 ****************************************************************************************************/
bool ReadDataFromBlock(MFRC522 &reader, int blockNum, byte readBlockData[]) {
  MFRC522::MIFARE_Key key;
  for (byte i = 0; i < 6; i++) key.keyByte[i] = 0xFF;  // Default key

  byte bufferLen = 18;
  MFRC522::StatusCode status;

  status = reader.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, blockNum, &key, &(reader.uid));
  if (status != MFRC522::STATUS_OK) {
    Serial.printf("❌ Auth failed: %s\n", reader.GetStatusCodeName(status));
    return false;
  }

  status = reader.MIFARE_Read(blockNum, readBlockData, &bufferLen);
  if (status != MFRC522::STATUS_OK) {
    Serial.printf("❌ Read failed: %s\n", reader.GetStatusCodeName(status));
    return false;
  }

  Serial.println("✅ Block read successfully.");
  return true;
}

/****************************************************************************************************
 * Beep Functions
 ****************************************************************************************************/
void BeepSuccess() {
  tone(BUZZER, 1000, 200);
  delay(250);
  tone(BUZZER, 1200, 200);
  delay(300);
  noTone(BUZZER);
}

void BeepError() {
  tone(BUZZER, 800, 500);
  delay(600);
  noTone(BUZZER);
}
