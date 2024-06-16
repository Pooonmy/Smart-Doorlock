#include <Wire.h>
#include <WiFi.h>
#include <Keypad.h>
#include <Keypad_I2C.h>
#include <PubSubClient.h>
#include <Adafruit_PN532.h>
#include <LiquidCrystal_I2C.h>
#include <TridentTD_LineNotify.h>

#define relayPin 4
#define bellPin 5
#define outButton 6
#define inButton 7
#define ledGreen 10
#define ledRed 11

#define I2CADDR 0x20
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  { '1', '2', '3', 'A' },
  { '4', '5', '6', 'B' },
  { '7', '8', '9', 'C' },
  { '*', '0', '#', 'D' }
};
byte rowPins[ROWS] = { 4, 5, 6, 7 };
byte colPins[COLS] = { 0, 1, 2, 3 };
Keypad_I2C keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS, I2CADDR, PCF8574);

#define PN532_IRQ 8
#define PN532_RESET 9
byte uid[] = { 0, 0, 0, 0 };
byte uidLength;
Adafruit_PN532 nfc(PN532_IRQ, PN532_RESET);

#define LCDaddress 0x27
#define LCDcols 16
#define LCDrows 2
LiquidCrystal_I2C lcd(LCDaddress, LCDcols, LCDrows);

//=================================================================================================
String camIP = "192.168.137.173";
char correctPasscode[] = "123456";
char enteredPasscode[7];
char newPasscode[7];

byte authorizedUID1[] = { 0xB3, 0x13, 0x14, 0x11 };
byte authorizedUID2[] = { 0xA3, 0x9B, 0xA2, 0x2F };
byte authorizedUID3[] = { 0xE1, 0x77, 0x71, 0x4B };
byte* authorizedUIDs[] = { authorizedUID1, authorizedUID2, authorizedUID3 };
int numAuthorizedUIDs = sizeof(authorizedUIDs) / sizeof(authorizedUIDs[0]);
//=================================================================================================
WiFiClient espClient;
PubSubClient client(espClient);
//=================================================================================================
const char* ssid = "*****"; //Replace with ur ssid and password
const char* password = "*****";
const char* lineToken = "*****";
//=================================================================================================
const char* mqtt_broker = "broker.hivemq.com";
const int mqtt_port = 1883;
//=================================================================================================

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  client.setServer(mqtt_broker, mqtt_port);
  client.setCallback(callback);
  while (!client.connected()) {
    String client_id = "esp32-client-";
    client_id += String(WiFi.macAddress());
    Serial.printf("The client %s connects to the public mqtt broker\n", client_id.c_str());
    if (client.connect(client_id.c_str()))
      Serial.println("Public emqx mqtt broker connected");
    else {
      Serial.print("failed with state ");
      Serial.print(client.state());
      delay(2000);
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  payload[length] = '\0';
  if (strcmp(topic, "poonmyDoorbell/unlock") == 0) {
    Serial.println((char*)payload);
    if (strcmp((char*)payload, "true") == 0) {
      unlockDoor();
    }
  }
}

bool checkUID(byte uid[], int uidLength) {
  for (int i = 0; i < numAuthorizedUIDs; i++) {  // Call compareUID function to compare current UID with authorizedUIDs
    if (compareUID(uid, uidLength, authorizedUIDs[i])) {
      return true;
    }
  }
  return false;
}

bool compareUID(byte uid[], int uidLength, byte* authorizedUID) {
  if (uidLength != sizeof(authorizedUID)) {  // If length of current UID isn't equal to authorizedUID
    return false;
  }
  for (int i = 0; i < uidLength; i++) { // Compare current UID with authorizedUID byte by byte
    if (uid[i] != authorizedUID[i]) {
      return false;
    }
  }
  return true;
}

void unlockDoor() {
  Serial.println("Door Unlocked");
  client.publish("poonmyDoorbell/bellStatus", "10");
  client.publish("poonmyDoorbell/doorStatus", "1");
  digitalWrite(relayPin, LOW);
  digitalWrite(ledRed, LOW);
  digitalWrite(ledGreen, HIGH);
  lcd.clear();
  lcd.setCursor(1, 0);
  lcd.print("Access Granted");
  LINE.notify("Door Unlocked");
  delay(3000);
  lcd.clear();
  client.publish("poonmyDoorbell/doorStatus", "0");
  digitalWrite(relayPin, HIGH);
  digitalWrite(ledRed, HIGH);
  digitalWrite(ledGreen, LOW);
}

void accessDenied() {
  lcd.clear();
  lcd.setCursor(1, 0);
  lcd.print("Access Denied");
  delay(1000);
  lcd.clear();
}

void setup() {
  Serial.begin(115200);
  Wire.begin();
  keypad.begin(makeKeymap(keys));
  nfc.begin();
  nfc.SAMConfig();
  setup_wifi();
  reconnect();
  lcd.init();
  lcd.clear();
  lcd.backlight();
  client.subscribe("poonmyDoorbell/#");
  client.publish("poonmyDoorbell/doorStatus", "0");
  client.publish("poonmyDoorbell/bellStatus", "10");
  LINE.setToken(lineToken);
  pinMode(inButton, INPUT_PULLUP);
  pinMode(outButton, INPUT_PULLUP);
  pinMode(bellPin, OUTPUT);
  pinMode(relayPin, OUTPUT);
  pinMode(ledGreen, OUTPUT);
  pinMode(ledRed, OUTPUT);
  Serial.println("Connected");
  LINE.notify("Device Connected");
  digitalWrite(relayPin, HIGH);
  digitalWrite(ledRed, HIGH);
  digitalWrite(ledGreen, LOW);
}

void loop() {
  client.loop();
  lcd.setCursor(0, 0);
  lcd.print("# to enter pass");
  lcd.setCursor(0, 1);
  lcd.print("* to change pass");
  char key = keypad.getKey();

  if (key == '#') {
    lcd.clear();
    while (strlen(enteredPasscode) < 6) {  // Accept 6 characters then proceed next
      char key = keypad.getKey();
      lcd.setCursor(0, 0);
      lcd.print("Enter Passcode");
      if (key != NO_KEY) {
        lcd.setCursor(strlen(enteredPasscode) * 2, 1);  // Set cursor with space between each number
        lcd.print(key);
        enteredPasscode[strlen(enteredPasscode)] = key;  // Append new character
      }
    }
    if (strcmp(enteredPasscode, correctPasscode) == 0) {  // Compare enteredPasscode with correctPasscode
      unlockDoor();
    } else {
      accessDenied();
    }
    memset(enteredPasscode, 0, sizeof(enteredPasscode));  // Reset enteredPasscode
  }

  if (key == '*') {
    lcd.clear();
    while (strlen(enteredPasscode) < 6) {  // Accept 6 characters then proceed next
      char key = keypad.getKey();
      lcd.setCursor(0, 0);
      lcd.print("Enter old pass");
      if (key != NO_KEY) {
        lcd.setCursor(strlen(enteredPasscode) * 2, 1);  // Set cursor with space between each number
        lcd.print(key);
        enteredPasscode[strlen(enteredPasscode)] = key;  // Append new character
      }
    }
    if (strcmp(enteredPasscode, correctPasscode) == 0) {  // Compare enteredPasscode with correctPasscode
      lcd.clear();
      while (strlen(newPasscode) < 6) {  // Accept 6 characters then proceed next
        char key = keypad.getKey();
        lcd.setCursor(0, 0);
        lcd.print("Enter new pass");
        if (key != NO_KEY) {
          lcd.setCursor(strlen(newPasscode) * 2, 1);  // Set cursor with space between each number
          lcd.print(key);
          newPasscode[strlen(newPasscode)] = key;  // Append new character
        }
      }
      strcpy(correctPasscode, newPasscode);  // Replace correctPasscode with newPasscode
      lcd.setCursor(0, 0);
      lcd.print("Passcode changed");
      lcd.setCursor(0, 1);
      lcd.print("Enter pass again");
      delay(2000);
      lcd.clear();
    } else {
      accessDenied();
    }
    memset(newPasscode, 0, sizeof(newPasscode));  // Reset newPasscode
    memset(enteredPasscode, 0, sizeof(enteredPasscode));  // Reset enteredPasscode
  }

  if (nfc.inListPassiveTarget()) {  // Wait for passive NFC/RFID tag to be present
    byte cardID = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength);  // Read the data on NFC/RFID tag then store in cardID
    if (cardID) {
      if (checkUID(uid, uidLength)) { //  Call checkUID function to check if UID is authorized or not
        Serial.println("Authorized Card");
        unlockDoor();
      } else {
        Serial.println("Unauthorized Card");
        accessDenied();
      }
    }
  }

  if (digitalRead(outButton) == LOW) {
    Serial.println("Bell ringed");
    client.publish("poonmyDoorbell/bellStatus", "11");
    digitalWrite(bellPin, HIGH);
    delay(300);
    digitalWrite(bellPin, LOW);
    LINE.notifySticker("There is someone in front of your house",2,34);
    LINE.notify("Image : http://" + camIP + "/jpg");
  }

  if (digitalRead(inButton) == LOW) {
    unlockDoor();
  }
}