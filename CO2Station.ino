#include <ESP8266WiFi.h>
#include "MHZ.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SimpleTimer.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include "FS.h"
#include "helper.h"
#include "icons.h"
#include "secrets.h"

#define DEBUG

#define MH_Z19_RX 2
#define MH_Z19_TX 16
#define BOZZER 14
#define LED_R 13
#define LED_G 15
#define LED_B 12

#define WIFI_CONNECTION_TIMEOUT 5
#define OLED_PAGE_INTERVAL 5000
#define OLED_PAGES 2
#define READ_SENSOR_INTERVAL 5000

#define OLED_PAGE_CO2_VALUE 0
#define OLED_PAGE_CO2_10_MIN_CHART 1

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP); //TODO: change NTP server to pool.ntp.org
Adafruit_SSD1306 display(-1);
MHZ co2(MH_Z19_RX, MH_Z19_TX, 1, MHZ19B);
WiFiClient client;
SimpleTimer timer;

int co2History[100];
int sensorTimer;
int preheatTimer;
int smartConfigLEDTimer;
bool smartConfigLEDState = false;
int oledTimer;
bool smartConfigRun = false;
unsigned int wifiConnectionTime = 0;
void (*oledPages[OLED_PAGES])();
byte currentOLEDPage = 0;
volatile bool buttonPressed = false;
volatile long lastButtonPressedMillis;
int ppm = 0;

void setup()
{
#ifdef DEBUG
  Serial.begin(9600);
#endif

  SPIFFS.begin();

  pinMode(0, INPUT_PULLUP);
  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);
  pinMode(BOZZER, OUTPUT);

  smartConfigLEDTimer = timer.setInterval(1000, smartConfigLED);
  timer.disable(smartConfigLEDTimer);

  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  WiFi.onEvent(WiFiEvent);
  WiFi.mode(WIFI_STA);
  WiFi.begin();

  if (WiFi.SSID() == "")
  {
    startSmartConfig();
  }

  blinkLEDOnStartup();

  attachInterrupt(digitalPinToInterrupt(0), handleButton, FALLING);

  initOledPages();

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.setTextColor(WHITE);

#ifdef CO2_DEBUG
  co2.setDebug(true);
#endif

  preheatTimer = timer.setInterval(1000, checkPreheat);
  sensorTimer = timer.setInterval(READ_SENSOR_INTERVAL, readSensor);
  oledTimer = timer.setInterval(OLED_PAGE_INTERVAL, showNextOledPage);
}

void loop()
{
  ArduinoOTA.handle();

  timeClient.update();

  timer.run();

  if (buttonPressed)
  {
    buttonPressed = false;
    timer.disable(oledTimer);
    showNextOledPage();
  }

  if (millis() - lastButtonPressedMillis > 10000)
  {
    timer.enable(oledTimer);
  }
}

void initOledPages()
{
  oledPages[OLED_PAGE_CO2_VALUE] = displayCO2;
  oledPages[OLED_PAGE_CO2_10_MIN_CHART] = displayCo2Plot;
}

void checkPreheat()
{
  if (co2.isPreHeating())
  {
#ifdef DEBUG
    Serial.print("Preheating...");
#endif
    displayPreheat();
  }
  else
  {
    timer.disable(preheatTimer);

    readSensor();

    showOledPage(OLED_PAGE_CO2_VALUE);

    beep(50, 100, 2, BOZZER);
  }
}

void WiFiEvent(WiFiEvent_t event)
{
#ifdef DEBUG
  Serial.printf("[WiFi-event] event: %d\n", event);
#endif
  switch (event)
  {
  case WIFI_EVENT_STAMODE_DISCONNECTED:
    wifiConnectionTime++;
    if (wifiConnectionTime > WIFI_CONNECTION_TIMEOUT)
    {
      startSmartConfig();
      wifiConnectionTime = 0;
    }
    break;
  case WIFI_EVENT_STAMODE_GOT_IP:
#ifdef DEBUG
    Serial.println("Got IP");
    Serial.println(WiFi.localIP());
#endif
    wifiConnectionTime = 0;   
    timer.disable(smartConfigLEDTimer);    
    analogWrite(LED_B, 0);    
    timeClient.begin();
    timeClient.setTimeOffset(7200);
    ArduinoOTA.begin();
    break;
  }
}

void startSmartConfig()
{
#ifdef DEBUG
  Serial.println("Smart config running");
#endif
  WiFi.beginSmartConfig();
  smartConfigRun = true;
  timer.enable(smartConfigLEDTimer);
}

void smartConfigLED()
{
  smartConfigLEDState = !smartConfigLEDState;
  analogWrite(LED_G, 0);
  analogWrite(LED_R, 0);
  analogWrite(LED_B, smartConfigLEDState ? 1024 : 0);
}

void blinkLEDOnStartup()
{
  analogWrite(LED_R, 0);
  analogWrite(LED_G, 0);
  analogWrite(LED_B, 0);

  for (int u = 0; u < 1024; u++)
  {
    analogWrite(LED_G, u);
    delayMicroseconds(100);
  }

  for (int u = 1024; u >= 0; u--)
  {
    analogWrite(LED_B, u);
    delayMicroseconds(100);
  }

  for (int u = 0; u < 1024; u++)
  {
    analogWrite(LED_R, u);
    delayMicroseconds(100);
  }

  for (int u = 1024; u >= 0; u--)
  {
    analogWrite(LED_G, u);
    delayMicroseconds(100);
  }

  analogWrite(LED_R, 0);
  analogWrite(LED_G, 0);
  analogWrite(LED_B, 0);
}

void handleButton()
{
#ifdef DEBUG
  Serial.println("Button pressed!");
#endif
  buttonPressed = true;
  lastButtonPressedMillis = millis();
}

void showOledPage(byte oledPageNumber)
{
  if (oledPageNumber >= OLED_PAGES || oledPageNumber < 0)
  {
    oledPageNumber = 0;
  }

  oledPages[oledPageNumber]();
}

void showNextOledPage()
{
  showOledPage(++currentOLEDPage);
}

void readSensor()
{
  byte attempts = 10;
  do
  {
    ppm = co2.readCO2UART();
    attempts--;
    delay(10);
  } while (ppm <= 0 && attempts > 0);

  for (byte i = 0; i < 99; i++)
  {
    co2History[i] = co2History[i + 1];
  }

  co2History[99] = ppm;
}

void displayPreheat()
{
  uint8_t preheadElapsedSeconds = 180 - (millis() / 1000);
  uint8_t elapsedSeconds;
  uint8_t elapsedMinutes;

  secondsToMS(preheadElapsedSeconds, elapsedMinutes, elapsedSeconds);

  static char timeString[] = "-:--";
  timeString[0] = '0' + elapsedMinutes % 10;
  timeString[2] = '0' + elapsedSeconds / 10;
  timeString[3] = '0' + elapsedSeconds % 10;

#ifdef DEBUG
  Serial.println(timeString);
#endif

  display.clearDisplay();
  display.setTextSize(3);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.drawBitmap(0, 0, heat_icon, 32, 32, 1);
  display.setCursor(48, 9);
  display.print(timeString);
  display.display();
}

void displayCO2()
{
  display.clearDisplay();
  display.drawBitmap(0, 0, co2_icon, 32, 32, 1);
  display.setTextSize(1);
  display.setCursor(48, 0);
  display.print("Internal");
  display.setTextSize(3);
  display.setCursor(46, 12);
  display.print(ppm);
  display.display();
}

void displayCo2Plot()
{
  int minimum = getMin(co2History, 100);
  minimum = minimum > 0 ? minimum : 1;

  int maximum = getMax(co2History, 100);
  maximum = maximum > 0 ? maximum : 1;

  display.clearDisplay();
  display.setTextSize(1);

  display.setCursor(30, 0);
  display.print("CO2 10m");

  display.setCursor(104, 0);
  display.print(maximum);

  display.setCursor(104, 24);
  display.print(minimum);

  for (byte i = 0; i < 100; i++)
  {
    int height = (co2History[i] * 22) / maximum;
    display.drawFastVLine(i, 32 - height, height, WHITE);
  }

  display.display();
}