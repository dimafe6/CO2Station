#if ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif

void secondsToMS(const uint32_t seconds, uint8_t &m, uint8_t &s)
{
    uint32_t t = seconds;
    s = t % 60;
    t = (t - s) / 60;
    m = t % 60;
    t = (t - m) / 60;
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

void beep(int duration, int pause, int beepsCount, byte pin)
{
  int i = 0;
  for (i = 0; i < beepsCount; i++)
  {
    delay(pause);
    digitalWrite(pin, HIGH);
    delay(duration);
    digitalWrite(pin, LOW);
  }
}

bool isTimeSynced(NTPClient& timeClient)
{
  return timeClient.getEpochTime() > 0;
}

String getFormattedTime(unsigned long rawTime)
{
  unsigned long hours = (rawTime % 86400L) / 3600;
  String hoursStr = hours < 10 ? "0" + String(hours) : String(hours);

  unsigned long minutes = (rawTime % 3600) / 60;
  String minuteStr = minutes < 10 ? "0" + String(minutes) : String(minutes);

  return hoursStr + ":" + minuteStr;
}