#include <ESP8266WiFi.h>
#include <SoftwareSerial.h>
#include "MHZ.h"
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ArduinoJson.h>
#include <SimpleTimer.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
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
#define OLED_PAGES 8
#define READ_SENSOR_INTERVAL 6000
#define UPDATE_WEATHER_INTERVAL 300000 // 5 minutes
#define TELEGRAM_BOT_INTERVAL 5000     // Mean time between scan messages

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
Adafruit_SSD1306 display(-1);
MHZ co2(MH_Z19_RX, MH_Z19_TX, 1, MHZ19B);
WiFiClient client;
WiFiClientSecure sslClient;
UniversalTelegramBot bot(BOTtoken, sslClient);
SimpleTimer timer;
StaticJsonBuffer<2000> weatherBuffer;

int co2History[100];
int wiFiTimer;
int sensorTimer;
int oledTimer;
int weatherTimer;
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
bool weatherUpdated = false;
bool ntpBegin = false;

struct Weather
{
  String city;
  float temp;
  int pressure;
  int humidity;
  float wind;
};

Weather weatherData;

void setup()
{
  pinMode(0, INPUT_PULLUP);
  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);
  pinMode(BOZZER, OUTPUT);

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

  attachInterrupt(digitalPinToInterrupt(0), handleButton, FALLING);

  oledPages[0] = displayCO2;
  oledPages[1] = displayTemperature;
  oledPages[2] = displayWeatherTemp;
  oledPages[3] = displayWeatherPressure;
  oledPages[4] = displayWeatherHumidity;
  oledPages[5] = displayWeatherWind;
  oledPages[6] = displayTime;
  oledPages[7] = displayCo2Plot;

  Serial.begin(9600);

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

    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(WHITE);
    display.setCursor(20, 0);
    display.print("Updating");
    display.setCursor(27, 18);
    display.print("weather");
    display.display();

    updateWeather();

    beep(50, 100, 2);
  }

  wiFiTimer = timer.setInterval(CHECK_WIFI_INTERVAL, checkWiFi);
  sensorTimer = timer.setInterval(READ_SENSOR_INTERVAL, readSensor);
  oledTimer = timer.setInterval(OLED_PAGE_INTERVAL, changeOledPage);
  weatherTimer = timer.setInterval(UPDATE_WEATHER_INTERVAL, updateWeather);
  telegramTimer = timer.setInterval(TELEGRAM_BOT_INTERVAL, telegramGetUpdates);
}

void loop()
{
  timeClient.update();

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

void updateWeather()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    weatherUpdated = false;
    return;
  }

  if (!client.connect("api.openweathermap.org", 80))
  {
    Serial.println("connection failed");
    weatherUpdated = false;
    return;
  }

  Serial.println("Update weather");

  client.println("GET /data/2.5/weather?id=703448&appid=0d05fb0926034f4a849664441742cf69 HTTP/1.1");
  client.println("Host: api.openweathermap.org");
  client.println("Connection: close");
  client.println();

  // wait for data to be available
  unsigned long timeout = millis();
  while (client.available() == 0)
  {
    yield();
    if (millis() - timeout > 5000)
    {
      Serial.println(">>> Client Timeout !");
      client.stop();
      weatherUpdated = false;
      return;
    }
  }

  String line;
  while (client.available())
  {
    line = client.readStringUntil('\r');
  }

  if (line.length() > 0)
  {
    Serial.println(line);
    JsonObject &root = weatherBuffer.parseObject(line);
    if (root.success())
    {
      String city = root["name"];
      weatherData.city = city;
      float tempK = root["main"]["temp"];
      float tempC = tempK - 273.15;
      weatherData.temp = tempC;
      int pressurehPa = root["main"]["pressure"];
      weatherData.pressure = pressurehPa / 1.333;
      weatherData.humidity = root["main"]["humidity"];
      weatherData.wind = root["wind"]["speed"];

      weatherUpdated = true;
    }
    else
    {
      Serial.println("parseObject() failed");
      weatherUpdated = false;
    }
  }
}

void changeOledPage()
{
  currentOLEDPage++;

  //Skip weather pages if weather is not updated
  if (!weatherUpdated && currentOLEDPage >= 2 && currentOLEDPage <= 5)
  {
    currentOLEDPage = 6;
    return;
  }

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

    if (!ntpBegin)
    {
      timeClient.begin();
      timeClient.setTimeOffset(7200);
      ntpBegin = true;
    }
  }
  else
  {
    ntpBegin = false;

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

bool isTimeSynced()
{
  timeClient.getEpochTime() > 0;
}

String getFormattedTime()
{
  unsigned long rawTime = timeClient.getEpochTime();
  unsigned long hours = (rawTime % 86400L) / 3600;
  String hoursStr = hours < 10 ? "0" + String(hours) : String(hours);

  unsigned long minutes = (rawTime % 3600) / 60;
  String minuteStr = minutes < 10 ? "0" + String(minutes) : String(minutes);

  return hoursStr + ":" + minuteStr;
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

void displayWeatherTemp()
{
  display.clearDisplay();
  display.drawBitmap(0, 0, term_icon, 9, 32, 1);
  display.setTextSize(1);
  display.setCursor(48, 0);
  display.print("Outside");
  display.setTextSize(3);
  display.setCursor(46, 12);
  display.print(String(weatherData.temp, 0));
  display.drawBitmap(100, 12, celsius_icon, 16, 16, 1);
  display.display();
}

void displayWeatherPressure()
{
  display.clearDisplay();
  display.drawBitmap(0, 0, atm_pressure_icon, 20, 32, 1);
  display.setTextSize(1);
  display.setCursor(48, 0);
  display.print("Outside");
  display.setTextSize(3);
  display.setCursor(46, 12);
  display.print(weatherData.pressure);
  display.display();
}

void displayWeatherHumidity()
{
  display.clearDisplay();
  display.drawBitmap(0, 0, humidity_icon, 28, 32, 1);
  display.setTextSize(1);
  display.setCursor(48, 0);
  display.print("Outside");
  display.setTextSize(3);
  display.setCursor(46, 12);
  display.print(weatherData.humidity);
  display.setTextSize(2);
  display.setCursor(104, 12);
  display.print("%");
  display.display();
}

void displayWeatherWind()
{
  display.clearDisplay();
  display.drawBitmap(0, 0, wind_icon, 32, 32, 1);
  display.setTextSize(1);
  display.setCursor(48, 0);
  display.print("Outside");
  display.setTextSize(3);
  display.setCursor(46, 12);
  display.print(String(weatherData.wind, 1));
  display.display();
}

void displayTime()
{
  display.clearDisplay();
  display.setTextSize(3);
  display.setCursor(20, 10);
  display.print(getFormattedTime());
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
    display.drawFastVLine(i, 32-height, height, WHITE);
  }

  display.display();
}