#include <ESP8266WiFi.h>
#include <SoftwareSerial.h>
#include "MHZ.h"
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SimpleTimer.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include "icons.h"
#include "secrets.h"

#define MH_Z19_RX 2
#define MH_Z19_TX 16
#define BOZZER 14
#define LED_R 13
#define LED_G 15
#define LED_B 12

#define WIFI_CONNECTION_TIMEOUT 10
#define CHECK_WIFI_INTERVAL 2000
#define SMART_CONFIG_TIMEOUT 300
#define OLED_PAGE_INTERVAL 5000
#define OLED_PAGES 3
#define READ_SENSOR_INTERVAL 6000
#define TELEGRAM_BOT_INTERVAL 5000 // Mean time between scan messages

Adafruit_SSD1306 display(-1);
MHZ co2(MH_Z19_RX, MH_Z19_TX, 1, MHZ19B);
WiFiClient client;
WiFiClientSecure sslClient;
UniversalTelegramBot bot(BOTtoken, sslClient);
SimpleTimer timer;

int co2History[100];
int wiFiTimer;
int sensorTimer;
int oledTimer;
int telegramTimer;
bool smartConfigRun = false;
unsigned int wifiConnectionTime = 0;
unsigned int smartConfigTime = 0;
void (*oledPages[OLED_PAGES])();
int currentOLEDPage = 0;
volatile bool buttonPressed = false;
volatile long lastButtonPressedMillis;
int ppm = 0;
int temp = 0;

void setup()
{
  Serial.begin(9600);

  pinMode(0, INPUT_PULLUP);
  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);
  pinMode(BOZZER, OUTPUT);

  analogWrite(LED_R, 0);
  analogWrite(LED_G, 0);
  analogWrite(LED_B, 0);

  blinkLEDOnStartup();

  attachInterrupt(digitalPinToInterrupt(0), handleButton, FALLING);

  oledPages[0] = displayCO2;
  oledPages[1] = displayTemperature;
  oledPages[2] = displayCo2Plot;

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);

  WiFi.mode(WIFI_STA);
  WiFi.begin();

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

    beep(50, 100, 2);
  }

  wiFiTimer = timer.setInterval(CHECK_WIFI_INTERVAL, checkWiFi);
  sensorTimer = timer.setInterval(READ_SENSOR_INTERVAL, readSensor);
  oledTimer = timer.setInterval(OLED_PAGE_INTERVAL, changeOledPage);
  telegramTimer = timer.setInterval(TELEGRAM_BOT_INTERVAL, telegramGetUpdates);
}

void loop()
{
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

void blinkLEDOnStartup()
{
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
}

int getMin(int *a, int size)
{
  int minimum = a[0];
  for (int i = 0; i < size; i++)
  {
    if (a[i] < minimum)
      minimum = a[i];
  }
  return minimum;
}

int getMax(int *a, int size)
{
  int maximum = a[0];
  for (int i = 0; i < size; i++)
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

void telegramGetUpdates()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    return;
  }

  int numNewMessages = 0;
  do
  {
    numNewMessages = bot.getUpdates(bot.last_message_received + 1);

    for (int i = 0; i < numNewMessages; i++)
    {
      bot.sendMessage(bot.messages[i].chat_id, bot.messages[i].text, "");
      numNewMessages--;
    }
  } while (numNewMessages);
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
  ppm = co2.readCO2UART();
  temp = co2.getLastTemperature();

  for (byte i = 0; i < 99; i++)
  {
    co2History[i] = co2History[i + 1];
  }
  co2History[99] = ppm;

  for (byte i = 0; i < 100; i++)
  {
    Serial.print(co2History[i]);
    Serial.print(",");
  }
  Serial.println();
}

void checkWiFi()
{
  if (WiFi.status() == WL_CONNECTED)
  {
    //Serial.println("WiFi connected!!!");
    wifiConnectionTime = 0;
    smartConfigRun = false;
    WiFi.stopSmartConfig();
  }
  else
  {
    if (!smartConfigRun)
    {
      //Serial.println("WiFi disconnected!!!");
      wifiConnectionTime++;
      if (wifiConnectionTime > WIFI_CONNECTION_TIMEOUT)
      {
        Serial.println("Smart config for 5 miutes");
        WiFi.beginSmartConfig();
        smartConfigRun = true;
        wifiConnectionTime = 0;
      }
    }
    else
    {
      smartConfigTime++;
      if (smartConfigTime > 60 && smartConfigRun)
      {
        WiFi.stopSmartConfig();
        WiFi.reconnect();
        smartConfigTime = 0;
        smartConfigRun = false;
        wifiConnectionTime = 0;
      }
    }
  }
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

void displayTemperature()
{
  display.clearDisplay();
  display.drawBitmap(0, 0, term_icon, 9, 32, 1);
  display.setTextSize(1);
  display.setCursor(48, 0);
  display.print("Internal");
  display.setTextSize(3);
  display.setCursor(46, 12);
  display.print(temp);
  display.drawBitmap(100, 12, celsius_icon, 16, 16, 1);
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