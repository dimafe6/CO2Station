#include <ESP8266WiFi.h>
#include "MHZ.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SimpleTimer.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include "helper.h"
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
#define OLED_PAGES 8
#define READ_SENSOR_INTERVAL 5000

#define OLED_PAGE_CO2_VALUE 0
#define OLED_PAGE_CO2_10_MIN_CHART 1
#define OLED_PAGE_TIME 2
#define OLED_PAGE_TEMPERATURE 3
#define OLED_PAGE_PRESSURE 4
#define OLED_PAGE_HUMIDITY 5
#define OLED_PAGE_TEMP_CHART 6
#define OLED_PAGE_HUMIDITY_CHART 7

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, 7200); //TODO: change NTP server to pool.ntp.org
Adafruit_SSD1306 display(-1);
MHZ co2(MH_Z19_RX, MH_Z19_TX, 1, MHZ19B);
WiFiClient client;
SimpleTimer timer;
Adafruit_BME280 bme;

int co2History[100];
int tempHistory[100];
int humHistory[100];
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
  Serial.begin(9600);

  initOledPages();

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

  bme.begin();

  if (WiFi.SSID() == "")
  {
    startSmartConfig();
  }

  blinkLEDOnStartup();

  attachInterrupt(digitalPinToInterrupt(0), handleButton, FALLING);

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
  oledPages[OLED_PAGE_TIME] = displayTime;
  oledPages[OLED_PAGE_TEMPERATURE] = displayTemperature;
  oledPages[OLED_PAGE_PRESSURE] = displayPressure;
  oledPages[OLED_PAGE_HUMIDITY] = displayHumidity;
  oledPages[OLED_PAGE_TEMP_CHART] = displayTempPlot;
  oledPages[OLED_PAGE_HUMIDITY_CHART] = displayHumPlot;
}

void checkPreheat()
{
  if (co2.isPreHeating())
  {
    Serial.print("Preheating...");
    displayPreheat();
  }
  else
  {
    timer.disable(preheatTimer);

    readSensor();

    showOledPage(OLED_PAGE_TIME);

    beep(50, 100, 2, BOZZER);
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
    timeClient.begin();
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

void handleButton()
{
  Serial.println("Button pressed!");
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
  currentOLEDPage++;

  if (currentOLEDPage >= OLED_PAGES || currentOLEDPage < 0)
  {
    currentOLEDPage = 0;
  }

  showOledPage(currentOLEDPage);
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
    tempHistory[i] = tempHistory[i + 1];
    humHistory[i] = humHistory[i + 1];
  }

  co2History[99] = ppm;
  tempHistory[99] = bme.readTemperature();
  humHistory[99] = bme.readHumidity();
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
  display.setCursor(48, 8);
  display.print(timeString);
  display.display();
}

void displayCO2()
{
  display.clearDisplay();
  display.drawBitmap(0, 0, co2_icon, 32, 32, 1);
  display.setTextSize(3);
  display.setCursor(46, 8);
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

void displayTempPlot()
{
  int minimum = getMin(tempHistory, 100);
  minimum = minimum > 0 ? minimum : 1;

  int maximum = getMax(tempHistory, 100);
  maximum = maximum > 0 ? maximum : 1;

  display.clearDisplay();
  display.setTextSize(1);

  display.setCursor(24, 0);
  display.print("Temp. 10m");

  display.setCursor(104, 0);
  display.print(maximum);

  display.setCursor(104, 24);
  display.print(minimum);

  for (byte i = 0; i < 100; i++)
  {
    int height = (tempHistory[i] * 22) / maximum;
    display.drawFastVLine(i, 32 - height, height, WHITE);
  }

  display.display();
}

void displayHumPlot()
{
  int minimum = getMin(humHistory, 100);
  minimum = minimum > 0 ? minimum : 1;

  int maximum = getMax(humHistory, 100);
  maximum = maximum > 0 ? maximum : 1;

  display.clearDisplay();
  display.setTextSize(1);

  display.setCursor(28, 0);
  display.print("Hum. 10m");

  display.setCursor(104, 0);
  display.print(maximum);

  display.setCursor(104, 24);
  display.print(minimum);

  for (byte i = 0; i < 100; i++)
  {
    int height = (humHistory[i] * 22) / maximum;
    display.drawFastVLine(i, 32 - height, height, WHITE);
  }

  display.display();
}

void displayTime()
{
  if (!isTimeSynced(timeClient))
  {
    showNextOledPage();
    return;
  }

  display.clearDisplay();
  display.setTextSize(3);
  display.setCursor(20, 8);
  display.print(getFormattedTime(timeClient.getEpochTime()));
  display.display();
}

void displayTemperature()
{
  display.clearDisplay();
  display.drawBitmap(0, 0, term_icon, 9, 32, 1);
  display.setTextSize(3);
  display.setCursor(24, 8);
  display.print(String(bme.readTemperature(), 1));
  display.drawBitmap(104, 8, celsius_icon, 16, 16, 1);
  display.display();
}

void displayPressure()
{
  display.clearDisplay();
  display.drawBitmap(0, 0, atm_pressure_icon, 20, 32, 1);
  display.setTextSize(3);
  display.setCursor(46, 8);
  display.print((int)(bme.readPressure() / 100.0F));
  display.display();
}

void displayHumidity()
{
  display.clearDisplay();
  display.drawBitmap(0, 0, humidity_icon, 28, 32, 1);
  display.setTextSize(3);
  display.setCursor(36, 8);
  display.print(String(bme.readHumidity(), 1));
  display.setTextSize(2);
  display.setCursor(110, 8);
  display.print("%");
  display.display();
}