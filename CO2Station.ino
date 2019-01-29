#include <ESP8266WiFi.h>
#include <SoftwareSerial.h>
#include "MHZ.h"
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <AmperkaFET.h>
#include <ArduinoJson.h>
#include <SimpleTimer.h>
#include "icons.h"

#define MH_Z19_RX 2
#define MH_Z19_TX 16
#define BOZZER_REGISTER 4
#define SHIFT_CS 15

#define WIFI_CONNECTION_TIMEOUT 10
#define CHECK_WIFI_INTERVAL 1000
#define SMART_CONFIG_TIMEOUT 300
#define OLED_PAGE_INTERVAL 5000
#define OLED_PAGES 3
#define READ_SENSOR_INTERVAL 5000
#define UPDATE_WEATHER_INTERVAL 3600000 //1 hour

Adafruit_SSD1306 display(-1);
MHZ co2(MH_Z19_RX, MH_Z19_TX, 1, MHZ19B);
FET shift(SHIFT_CS);
WiFiClient client;
SimpleTimer timer;
StaticJsonBuffer<2000> weatherBuffer;

int wiFiTimer;
int sensorTimer;
int oledTimer;
int weatherTimer;
bool smartConfigRun = false;
bool initialWeatherUpdate = false;
unsigned int wifiConnectionTime = 0;
unsigned int smartConfigTime = 0;
void (*oledPages[OLED_PAGES])();
int currentOLEDPage = 0;
int ppm = 0;
int temp = 0;

struct Weather
{
  String city;
  float temp;
  float pressure;
  int humidity;
  float wind;
};

Weather weatherData;

void setup()
{
  oledPages[0] = displayCO2;
  oledPages[1] = displayTemperature;
  oledPages[2] = displayWeather;

  Serial.begin(9600);

  shift.begin();

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);

  wiFiTimer = timer.setInterval(CHECK_WIFI_INTERVAL, checkWiFi);
  sensorTimer = timer.setInterval(READ_SENSOR_INTERVAL, readSensor);
  oledTimer = timer.setInterval(OLED_PAGE_INTERVAL, changeOledPage);
  weatherTimer = timer.setInterval(UPDATE_WEATHER_INTERVAL, updateWeather);

  WiFi.mode(WIFI_STA);
  WiFi.begin();

#ifdef CO2_DEBUG
  co2.setDebug(true);
#endif

  display.clearDisplay();

  if (co2.isPreHeating())
  {
    Serial.print("Preheating");

    beep(70, 100, 1);

    while (co2.isPreHeating())
    {
      displayPreheat();

      delay(1000);
    }
    beep(50, 100, 2);
  }
}

void loop()
{
  timer.run();
}

void updateWeather()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    return;
  }

  if (!client.connect("api.openweathermap.org", 80))
  {
    Serial.println("connection failed");
    return;
  }

  Serial.println("Update weather");

  client.println("GET /data/2.5/weather?id=703448&appid=0d05fb0926034f4a849664441742cf69 HTTP/1.1");
  client.println("Host: api.openweathermap.org");
  client.println("Connection: close");
  client.println();

  delay(1500);
  String line;
  while (client.available())
  {
    line = client.readStringUntil('\r');
  }
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
  }
  else
  {
    Serial.println("parseObject() failed");
  }
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
  if (ppm > 0)
  {
    Serial.print("PPM: ");
    Serial.println(ppm);
  }

  temp = co2.getLastTemperature();
  if (temp > 0)
  {
    Serial.print("Temperature: ");
    Serial.println(temp);
  }
}

void checkWiFi()
{
  if (WiFi.status() == WL_CONNECTED)
  {
    //Serial.println("WiFi connected!!!");
    wifiConnectionTime = 0;
    smartConfigRun = false;
    WiFi.stopSmartConfig();

    if (!initialWeatherUpdate)
    {
      initialWeatherUpdate = true;
      updateWeather();
    }
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
    shift.digitalWrite(BOZZER_REGISTER, HIGH);
    delay(duration);
    shift.digitalWrite(BOZZER_REGISTER, LOW);
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
  display.drawBitmap(0, 0, co2_icon, 40, 32, 1);
  display.setTextSize(3);
  display.setCursor(52, 11);
  display.print(ppm);
  display.setTextSize(1);
  display.setCursor(110, 0);
  display.print("ppm");
  display.display();
}

void displayTemperature()
{
  display.clearDisplay();
  display.drawBitmap(10, 0, term_icon, 9, 32, 1);
  display.setTextSize(4);
  display.setCursor(44, 4);
  display.print(temp);
  display.drawBitmap(94, 0, celsius_icon, 16, 16, 1);
  display.display();
}

void displayWeather()
{
  Serial.println("City: " + String(weatherData.city));
  Serial.println("Temp: " + String(weatherData.temp));
  Serial.println("Humidity: " + String(weatherData.humidity));
  Serial.println("Pressure: " + String(weatherData.pressure));
  Serial.println("Wind: " + String(weatherData.wind));
}