#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <Arduino_JSON.h>
#include <assert.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <TimeLib.h>
#include <ESP32Servo.h>

#define SS_PIN  5  // ESP32 pin GPIO5 
#define RST_PIN 4 // ESP32 pin GPIO4
#define SENSOR  27

static const int servoPin = 13; //pin servo

volatile byte pulseCount;

const char input[] = "{\" c0 7e 6e a3\":\"Hasbian syukron\", \" 53 84 e1 e4\":\"Hasbian syukron2\"}";
const char* ssid = "AndroidAPEC2C";
const char* password = "oktk7017";
const char* serverName = "https://7c1fc9f7-f630-4b0f-87c0-a07ebffa12f9-00-lucirahvqeba.sisko.replit.dev/send?";

unsigned int flowMilliLitres;
unsigned long totalMilliLitres;

long currentMillis = 0;
long previousMillis = 0;

float calibrationFactor = 5.35;
float flowRate;
float Liter;

int interval = 1000;

bool state = false;

char buf[6];

byte pulse1Sec = 0;
String data = "";

MFRC522 rfid(SS_PIN, RST_PIN);
LiquidCrystal_I2C lcd(0x27, 20, 4);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 7 * 3600, 60000);  // UTC + 7 jam dan update setiap 60 detik
Servo servo1;

void IRAM_ATTR pulseCounter()
{
  pulseCount++;
}

void init_flow(){
  pinMode(SENSOR, INPUT_PULLUP);
  pulseCount = 0;
  flowRate = 0.0;
  flowMilliLitres = 0;
  totalMilliLitres = 0;
  Liter = 0;
  previousMillis = 0;
  attachInterrupt(digitalPinToInterrupt(SENSOR), pulseCounter, FALLING);
}

void show_display(int coloum, int rows, String text){
  lcd.setCursor(coloum, rows);
  lcd.print(text);
}

String take_name(String key) {
  JSONVar myObject = JSON.parse(input);
  if (JSON.typeof(myObject) == "undefined") {
    Serial.println("Parsing input failed!");
    return "json not found";
  }
  if (myObject.hasOwnProperty(key)) {
    return (const char*)myObject[key];
  }
  return "Key not found";
}

String readRFID() {
  String result = "";
  if (rfid.PICC_IsNewCardPresent()) { // new tag is available
    if (rfid.PICC_ReadCardSerial()) { // NUID has been read
      MFRC522::PICC_Type piccType = rfid.PICC_GetType(rfid.uid.sak);
      for (int i = 0; i < rfid.uid.size; i++) {
        result += (rfid.uid.uidByte[i] < 0x10 ? " 0" : " ");
        result += String(rfid.uid.uidByte[i], HEX);
      }
      rfid.PICC_HaltA(); // halt PICC
      rfid.PCD_StopCrypto1(); // stop encryption on PCD
    }
  }
  return result;
}

void init_WIFI(){
  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Connected!");
}

void Send_data(String nama, String tanggal, String jam, String volume){
  // Make the POST request
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;

    // Specify the URL and start the request
    http.begin(serverName);

    // Specify the content type
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");

    String postData = "nama=" + nama + "&tanggal=" + tanggal + "&jam=" + jam + "&volume=" + volume;

    // Send the POST request
    int httpResponseCode = http.POST(postData);

    // Check the response code
    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.println("HTTP Response code: " + String(httpResponseCode));
      Serial.println("Response: " + response);
    } else {
      Serial.println("Error on sending POST: " + String(httpResponseCode));
    }

    // Close the connection
    http.end();
  } else {
    Serial.println("Error in WiFi connection");
  }
}

String take_tanggal(){
  timeClient.update();
  unsigned long epochTime = timeClient.getEpochTime();
  setTime(epochTime);
  return String(day()) + "/" + String(month()) + "/" + String(year());
}

String take_jam(){
  timeClient.update();
  unsigned long epochTime = timeClient.getEpochTime();
  setTime(epochTime);
  return String(hour()) + ":" + String(minute()) + ":" + String(second());
}

void setup() {
  Serial.begin(115200);
  SPI.begin(); // init SPI bus
  rfid.PCD_Init(); // init MFRC522
  init_flow();
  init_WIFI();
  timeClient.begin();
  lcd.init();
  lcd.backlight();
  Serial.println("Tap an RFID/NFC tag on the RFID-RC522 reader");
  lcd.clear();
  show_display(0, 0, "Welcome!");
  pinMode(14, INPUT_PULLUP);
  servo1.attach(servoPin);
  servo1.write(0); //nutup
}

void loop() {
  String rfidInfo = readRFID();
  if (rfidInfo != "") {
    state = true;
    data = rfidInfo;
    servo1.write(180);
    Serial.println(data);
  }
  if (state) {
    // Serial.println("Masuk logika ini");
    int button_kon = digitalRead(14);
    currentMillis = millis();
    if (currentMillis - previousMillis > interval) {
    
      pulse1Sec = pulseCount;
      pulseCount = 0;
      flowRate = ((1000.0 / (millis() - previousMillis)) * pulse1Sec) / calibrationFactor;
      previousMillis = millis();
      // flowMilliLitres = (flowRate / 60) * 1000;
      flowMilliLitres = (flowRate / 60) * 1000;
      totalMilliLitres += flowMilliLitres;
      Liter = float(totalMilliLitres)/1000;
      dtostrf(Liter,5,2,buf);
      // Print the flow rate for this second in litres / minute
      Serial.print("Flow rate: ");
      Serial.print(int(flowRate));  // Print the integer part of the variable
      Serial.print("L/min");
      Serial.print("\t");       // Print tab space

      // Print the cumulative total of litres flowed since starting
      Serial.print("Output Liquid Quantity: ");
      Serial.print(totalMilliLitres);
      Serial.print("mL / ");
      Serial.print(Liter);
      Serial.println("L");
    }
    if (button_kon == LOW){
      Send_data(data, take_tanggal(), take_jam(), String(Liter, 2) + "L");
      state = false;
      totalMilliLitres = 0;
      data = "";
      servo1.write(0);
      lcd.clear();
      show_display(7, 1, "Sukses");
      show_display(6, 2, "Mengirim");
      delay(5000);
    }
    lcd.clear();
    show_display(0, 0, "Flowrate:");
    show_display(0, 1, String(flowMilliLitres));
    show_display(5, 1, "mL/s");
    show_display(0, 2, "Total:");
    show_display(0, 3, buf);
    show_display(5, 3, "Liter");
  }
  else{
    // Serial.println("Masuk ke logika ini");
    lcd.clear();
    show_display(6, 2, "Silahkan Tap");
    show_display(7, 3, "Kartu");
  }
  delay(100);
}
