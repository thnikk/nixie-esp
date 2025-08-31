#include <Arduino.h>
#include <SPI.h>
#include <WiFi.h>
#include <time.h>
#include <ArduinoOTA.h>
#include "credentials.h"

const byte LEpin=12; //pin Latch Enabled data accepted while HI level

String stringToDisplay="012345";// Content of this string will be displayed on tubes (must be 6 chars length)
static unsigned int SymbolArray[10]={1, 2, 4, 8, 16, 32, 64, 128, 256, 512};

// Dot stuff
#define UpperDotsMask 0x80000000
#define LowerDotsMask 0x40000000
static bool dots = 1; // enable/disable dots
static bool dotsBlink = 1; // enable/disable blinking
static bool topDots = 0; // enable/disable top dots
static bool bottomDots = 1; // enable/disable bottom dots

// Brightness
uint8_t check = 0;
uint8_t brightness = 5;
uint8_t duty_cycle = 5;

// Time
const char* ntpServer = "pool.ntp.org";
unsigned long lastNtpSync = 0;
const unsigned long ntpInterval = 3600 * 1000; // 1 hour

void setup()
{
  Serial.begin(115200);
  Serial.println(F("Starting"));

  pinMode(LEpin, OUTPUT);

  // SPI setup
  SPI.begin(); //
  SPI.setDataMode (SPI_MODE2); // Mode 2 SPI
  SPI.setClockDivider(SPI_CLOCK_DIV8); // SCK = 16MHz/8= 2MHz

  // Wifi
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");

  // Configure time with automatic DST from TZ_INFO
  configTzTime(TZ_INFO, ntpServer);
  syncTimeFromNTP();

  // OTA setup
  ArduinoOTA.setHostname("nixie-clock");   // shows up in PlatformIO
  ArduinoOTA
    .onStart([]() {
      Serial.println("Start updating");
    })
    .onEnd([]() {
      Serial.println("\nUpdate complete");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      int percent = (progress * 100) / total;
      char buf[7];
      snprintf(buf, sizeof(buf), "%6d", percent); // pad to 6 chars
      stringToDisplay = buf;
      doIndication();
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });

  ArduinoOTA.begin();
  Serial.println("OTA ready");
}

unsigned long lastUpdate = millis();
unsigned long dotUpdate = millis();

void loop() {
  ArduinoOTA.handle();

  if (millis() - lastNtpSync > ntpInterval) {
    syncTimeFromNTP();
  }

  if (millis() - lastUpdate > 1000) {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
      char timeStr[7]; // hhmmss
      strftime(timeStr, sizeof(timeStr), "%I%M%S", &timeinfo);
      stringToDisplay = timeStr;
    }
    dots = dots ? 0 : 1;
    lastUpdate = millis();
  }

  doIndication();
  delay(1);
}

void syncTimeFromNTP() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return;
  }
  lastNtpSync = millis();
  Serial.println("Time synced from NTP");
}

void doIndication()
{
  digitalWrite(LEpin, LOW);    // allow data input (Transparent mode)
  unsigned long Var32=0;

  long digits=stringToDisplay.toInt();

  //-------- REG 1 -----------------------------------------------
  Var32=0;

  Var32|=(unsigned long)(SymbolArray[digits%10])<<20; // s2
  digits=digits/10;

  Var32|=(unsigned long)(SymbolArray[digits%10])<<10; //s1
  digits=digits/10;

  Var32|=(unsigned long) (SymbolArray[digits%10]); //m2
  digits=digits/10;

  if (dots){
    if (topDots) Var32|=UpperDotsMask;
    if (bottomDots) Var32|=LowerDotsMask;
  }

  // PWM-ish brightness
  if (check >= brightness) {
    Var32=0;
  }

  SPI.transfer(Var32>>24);
  SPI.transfer(Var32>>16);
  SPI.transfer(Var32>>8);
  SPI.transfer(Var32);

  //-------- REG 0 -----------------------------------------------
  Var32=0;

  Var32|=(unsigned long)(SymbolArray[digits%10])<<20; // m1
  digits=digits/10;

  Var32|= (unsigned long)(SymbolArray[digits%10])<<10; //h2
  digits=digits/10;

  Var32|= (unsigned long)SymbolArray[digits%10]; //h1
  digits=digits/10;

  if (dots){
    if (topDots) Var32|=UpperDotsMask;
    if (bottomDots) Var32|=LowerDotsMask;
  }

  // PWM-ish brightness
  if (check >= brightness) {
    Var32=0;
  }
  check++;
  if (check > duty_cycle) check = 0;

  SPI.transfer(Var32>>24);
  SPI.transfer(Var32>>16);
  SPI.transfer(Var32>>8);
  SPI.transfer(Var32);

  digitalWrite(LEpin, HIGH);     // latching data
}


byte decToBcd(byte val) { return ( (val / 10 * 16) + (val % 10) ); }  // Convert normal decimal numbers to binary coded decimal

byte bcdToDec(byte val)  { return ( (val / 16 * 10) + (val % 16) ); }  // Convert binary coded decimal to normal decimal numbers

