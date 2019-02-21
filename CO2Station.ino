#include <ESP8266WiFi.h>
#include <SoftwareSerial.h>
#include "MHZ.h"
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SimpleTimer.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include "icons.h"
#include "secrets.h"

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

Adafruit_SSD1306 display(-1);
MHZ co2(MH_Z19_RX, MH_Z19_TX, 1, MHZ19B);
WiFiClient client;
SimpleTimer timer;

int co2History[100];
int sensorTimer;
int smartConfigLEDTimer;
bool smartConfigLEDState = false;
int oledTimer;
bool smartConfigRun = false;
unsigned int wifiConnectionTime = 0;
void (*oledPages[OLED_PAGES])();
int currentOLEDPage = 0;
volatile bool buttonPressed = false;
volatile long lastButtonPressedMillis;
int ppm = 0;

void setup()
{
  Serial.begin(9600);

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

  blinkLEDOnStartup();

  attachInterrupt(digitalPinToInterrupt(0), handleButton, FALLING);

  oledPages[0] = displayCO2;
  oledPages[1] = displayCo2Plot;

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);

#ifdef CO2_DEBUG
  co2.setDebug(true);
#endif

  display.clearDisplay();
  display.setTextColor(WHITE);

  if (co2.isPreHeating())
  {
    Serial.print("Preheating");

    beep(70, 100, 1);

    while (co2.isPreHeating())
    {
      delay(1000);
      displayPreheat();
    }

    readSensor();

    changeOledPage();

    beep(50, 100, 2);
  }

  if(WiFi.SSID() == "") {
    startSmartConfig();
  }

  sensorTimer = timer.setInterval(READ_SENSOR_INTERVAL, readSensor);
  oledTimer = timer.setInterval(OLED_PAGE_INTERVAL, changeOledPage);
}

void loop()
{
  ArduinoOTA.handle();

  timer.run();
  if (buttonPressed)
  {
    buttonPressed = false;
    timer.disable(oledTimer);
    changeOledPage();
  }

  if (millis() - lastButtonPressedMillis > 10000)
  {
    timer.enable(oledTimer);
  }
}

void WiFiEvent(WiFiEvent_t event)
{
  Serial.printf("[WiFi-event] event: %d\n", event);

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
    Serial.println("Got IP");
    Serial.println(WiFi.localIP());
    wifiConnectionTime = 0;
    timer.disable(smartConfigLEDTimer);
    analogWrite(LED_B, 0);
    ArduinoOTA.begin();
    break;
  }
}

void startSmartConfig()
{
  Serial.println("Smart config running");
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

int getMin(int *a, int size)
{
  int minimum = a[0];
  for (int i = 1; i < size; i++)
  {
    if (a[i] < minimum)
      minimum = a[i];
  }
  return minimum;
}

int getMax(int *a, int size)
{
  int maximum = a[0];
  for (int i = 1; i < size; i++)
  {
    if (a[i] > maximum)
      maximum = a[i];
  }
  return maximum;
}

void handleButton()
{
  Serial.println("Button pressed!");
  buttonPressed = true;
  lastButtonPressedMillis = millis();
}

void changeOledPage()
{
  currentOLEDPage++;

  if (currentOLEDPage >= OLED_PAGES)
  {
    currentOLEDPage = 0;
  }

  oledPages[currentOLEDPage]();
}

void readSensor()
{
  byte attempts = 10;
  do
  {
    ppm = co2.readCO2UART();
    attempts--;
    delay(10);
  } while (ppm <= 0 && attempts>0);

  for (byte i = 0; i < 99; i++)
  {
    co2History[i] = co2History[i + 1];
  }

  co2History[99] = ppm;
}

void beep(int duration, int pause, int beepsCount)
{
  int i = 0;
  for (i = 0; i < beepsCount; i++)
  {
    delay(pause);
    digitalWrite(BOZZER, HIGH);
    delay(duration);
    digitalWrite(BOZZER, LOW);
  }
}

void secondsToMS(const uint32_t seconds, uint8_t &m, uint8_t &s)
{
  uint32_t t = seconds;
  s = t % 60;
  t = (t - s) / 60;
  m = t % 60;
  t = (t - m) / 60;
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

  Serial.println(timeString);

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