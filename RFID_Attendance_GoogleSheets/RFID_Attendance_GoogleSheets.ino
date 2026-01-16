// Smart Attendance System with Face Trigger + RFID + Google Sheets
// ✅ ESP8266 + RFID + I2C LCD (Clean Serial for Python Integration)

#include <SPI.h>
#include <MFRC522.h>
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>
#include <Wire.h>
#include <LiquidCrystal_PCF8574.h>

//-----------------------------------------
#define RST_PIN  D3
#define SS_PIN   D4
#define BUZZER   D8
//-----------------------------------------
MFRC522 mfrc522(SS_PIN, RST_PIN);
MFRC522::MIFARE_Key key;  
MFRC522::StatusCode status;      
//-----------------------------------------
int blockNum = 2;  
byte bufferLen = 18;
byte readBlockData[18];
//-----------------------------------------
String card_holder_name;
const String sheet_url = "https://script.google.com/macros/s/AKfycbyrrQL5hmBA7u5nrcAI2kSt60zMg9ACABlwYgNCS2G4rkREAxiJke6kHnQC2pBwWL2N/exec?name=";

//-----------------------------------------
#define WIFI_SSID "HKs Domain"
#define WIFI_PASSWORD "oneforall"

//-----------------------------------------
LiquidCrystal_PCF8574 lcd(0x27);  // I2C LCD address
bool rfidEnabled = false;         // Controlled by Python

/****************************************************************************************************
 * setup() function
 ****************************************************************************************************/
void setup()
{
  delay(500);                 
  Serial.begin(115200);        
  Serial.setDebugOutput(false); // ✅ Disable WiFi debug logs
  Serial.flush();               // ✅ Clear any previous buffer

  Serial.println("\n================================");
  Serial.println("✅ NodeMCU Booting...");
  Serial.println("================================");

  // ✅ Initialize LCD
  lcd.begin(16, 2);      
  lcd.setBacklight(255); 
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("  Initializing  ");
  for (int a = 5; a <= 10; a++) {
    lcd.setCursor(a, 1);
    lcd.print(".");
    delay(400);
  }

  // ✅ WiFi Setup with timeout
  Serial.print("Connecting to WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  uint32_t startAttempt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 15000) {
    Serial.print(".");
    delay(300);
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.println("WiFi connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n⚠️ WiFi not connected! Will retry in loop.");
  }

  // ✅ Setup buzzer
  pinMode(BUZZER, OUTPUT);

  // ✅ Initialize SPI + RFID
  SPI.begin();
  mfrc522.PCD_Init();
  delay(500);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Waiting for Face");
  lcd.setCursor(0, 1);
  lcd.print("Verification...");

  Serial.println("NODEMCU_READY"); // 🔹 Python will wait for this
}

/****************************************************************************************************
 * loop() function
 ****************************************************************************************************/
void loop()
{
  // Step 1: Check Serial for START command from Python
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();

    if (command == "START") {
      rfidEnabled = true;
      Serial.println("START received → RFID Enabled!");
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Scan your Card ");
      delay(200);
    }
  }

  // Step 2: If RFID is enabled, scan card
  if (rfidEnabled) {
    if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {
      return;  // No card yet
    }

    // ✅ Card detected: print UID
    Serial.print("Card UID:");
    for (byte i = 0; i < mfrc522.uid.size; i++) {
      Serial.print(" ");
      if (mfrc522.uid.uidByte[i] < 0x10) Serial.print("0");
      Serial.print(mfrc522.uid.uidByte[i], HEX);
    }
    Serial.println();

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Card Detected!");

    Serial.println(F("Reading last data from RFID..."));
    ReadDataFromBlock(blockNum, readBlockData);

    Serial.print(F("Last data in RFID block "));
    Serial.print(blockNum);
    Serial.print(F(" --> "));
    for (int j = 0; j < 16; j++) {
      Serial.write(readBlockData[j]);
    }
    Serial.println();

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Hey " + String((char*)readBlockData) + "!");

    // Beep Buzzer
    for(int i=0; i<2; i++) {
      digitalWrite(BUZZER, HIGH); delay(200);
      digitalWrite(BUZZER, LOW);  delay(200);
    }

    // Step 3: Send to Google Sheets
    if (WiFi.status() == WL_CONNECTED) {
      std::unique_ptr<BearSSL::WiFiClientSecure>client(new BearSSL::WiFiClientSecure);
      client->setInsecure(); // Ignore SSL certificate
      card_holder_name = sheet_url + String((char*)readBlockData);
      card_holder_name.trim();
      Serial.println(card_holder_name);

      HTTPClient https;
      https.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
      Serial.println(F("[HTTPS] begin..."));

      if (https.begin(*client, card_holder_name)){
        Serial.println(F("[HTTPS] GET..."));
        int httpCode = https.GET();

        if (httpCode > 0) {
          Serial.printf("[HTTPS] GET... code: %d\n", httpCode);
          lcd.setCursor(0, 1);
          lcd.print(" Data Recorded ");
          delay(2000);
        } else {
          Serial.printf("[HTTPS] GET... failed, error: %s\n", https.errorToString(httpCode).c_str());
        }
        https.end();
        delay(500);
      } else {
        Serial.println("[HTTPS] Unable to connect");
      }
    }

    // ✅ Inform via Serial for debugging
    Serial.println("CARD_SCANNED");

    // Step 4: Disable RFID until next face verification
    rfidEnabled = false;
    Serial.println("RFID Scan Complete. Waiting for START again...");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Waiting for Face");
    lcd.setCursor(0, 1);
    lcd.print("Verification...");
    delay(200);
  }
}

/****************************************************************************************************
 * ReadDataFromBlock() function
 ****************************************************************************************************/
void ReadDataFromBlock(int blockNum, byte readBlockData[]) 
{ 
  for (byte i = 0; i < 6; i++) {
    key.keyByte[i] = 0xFF;
  }

  status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, blockNum, &key, &(mfrc522.uid));
  if (status != MFRC522::STATUS_OK){
     Serial.print("Authentication failed for Read: ");
     Serial.println(mfrc522.GetStatusCodeName(status));
     return;
  } else {
    Serial.println("Authentication success");
  }

  status = mfrc522.MIFARE_Read(blockNum, readBlockData, &bufferLen);
  if (status != MFRC522::STATUS_OK) {
    Serial.print("Reading failed: ");
    Serial.println(mfrc522.GetStatusCodeName(status));
    return;
  } else {
    Serial.println("Block was read successfully");  
  }
}
