#include <ESP8266WiFi.h>
#include <SoftwareSerial.h>
#include "MHZ.h"
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <AmperkaFET.h>
#include "icons.h"

#define MH_Z19_RX 2
#define MH_Z19_TX 16
#define BOZZER_REGISTER 4
#define SHIFT_CS 15

Adafruit_SSD1306 display(-1);
MHZ co2(MH_Z19_RX, MH_Z19_TX, 1, MHZ19B);
FET shift(SHIFT_CS);
WiFiClient client;

WiFiEventHandler connectedHandler;
WiFiEventHandler disconnectedHandler;

void setup()
{
  Serial.begin(9600);

  shift.begin();

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);

  connectedHandler = WiFi.onStationModeConnected(&onStationConnected);
  disconnectedHandler = WiFi.onStationModeDisconnected(&onStationDisconnected);

  smartConfig();

  co2.setDebug(true);

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
  int ppm = co2.readCO2UART();
  if (ppm > 0)
  {
    displayCO2(ppm);

    Serial.print("PPM: ");
    Serial.println(ppm);

    delay(5000);
  }

  int temperature = co2.getLastTemperature();
  if (temperature > 0)
  {
    displayTemperature(temperature);

    Serial.print("Temperature: ");
    Serial.println(temperature);

    delay(5000);
  }
  delay(1000);
}

void onStationConnected(const WiFiEventStationModeConnected &evt)
{
  Serial.print("Station connected: ");
  Serial.println(evt.ssid);
}

void onStationDisconnected(const WiFiEventStationModeDisconnected &evt)
{
  Serial.print("Station disconnected: ");
  Serial.println(evt.ssid);
}

void smartConfig()
{
  WiFi.mode(WIFI_STA);
  delay(500);

  WiFi.beginSmartConfig();
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

void displayCO2(int ppm)
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

void displayTemperature(int temp)
{
  display.clearDisplay();
  display.drawBitmap(10, 0, term_icon, 9, 32, 1);
  display.setTextSize(4);
  display.setCursor(44, 4);
  display.print(temp);
  display.drawBitmap(94, 0, celsius_icon, 16, 16, 1);
  display.display();
}