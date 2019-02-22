#include <ESP8266WiFi.h>

const unsigned char celsius_icon[] PROGMEM = {
    B00000000, B00000000,
    B00000000, B00000000,
    B00110000, B01111100,
    B01001000, B11000010,
    B01001001, B10000001,
    B01001001, B00000000,
    B00110001, B00000000,
    B00000001, B00000000,
    B00000001, B00000000,
    B00000001, B00000000,
    B00000001, B00000000,
    B00000001, B00000000,
    B00000001, B10000001,
    B00000000, B11000010,
    B00000000, B01111100,
    B00000000, B00000000};

const unsigned char co2_icon[] PROGMEM = {
    B00000000, B00000000, B01111000, B00000000,
    B00000000, B00000000, B11001100, B00000000,
    B00000000, B00000001, B10000110, B00000000,
    B00000000, B00000011, B00000011, B11100000,
    B00000000, B00000110, B00000000, B00100000,
    B00000000, B00000100, B00000000, B00110000,
    B00000000, B00011100, B00000000, B00011000,
    B00000000, B00111000, B00000000, B00001100,
    B00000000, B01100011, B11111000, B00000100,
    B00000000, B01000110, B00001110, B00000010,
    B00000000, B11001100, B00000011, B00000010,
    B00000001, B11111000, B00000001, B00000010,
    B00000011, B00000000, B00000001, B10000011,
    B00000010, B00000000, B00000000, B10000011,
    B00000110, B00000000, B00000000, B11000011,
    B00001100, B00000000, B00000000, B01110110,
    B00011000, B00000000, B00000000, B00011100,
    B00110000, B00111100, B11100000, B00001100,
    B01100000, B01000101, B00010000, B00000100,
    B01000000, B01000001, B00010000, B00000110,
    B01000000, B01000001, B00010000, B00000010,
    B11000000, B01000001, B00010111, B00000010,
    B11000000, B01000001, B00010101, B00000010,
    B11000000, B01000001, B00010001, B00000011,
    B11000000, B01000101, B00010010, B00000011,
    B01000000, B00111101, B11100100, B00000011,
    B01000000, B00000000, B00000111, B00000011,
    B01100000, B00000000, B00000000, B00000110,
    B00110000, B00000000, B00000000, B00001100,
    B00011100, B00000000, B00000000, B00011000,
    B00001111, B11111111, B11111111, B11110000,
    B00000011, B11111111, B11111111, B11000000};

const unsigned char heat_icon[] PROGMEM = {
    B00000000, B00000000, B00000000, B00000000,
    B00000000, B00000001, B10000000, B00000000,
    B00000000, B00000011, B11000000, B00000000,
    B00000000, B00000011, B11000000, B00000000,
    B00000000, B00000110, B01100000, B00000000,
    B00000000, B00000100, B00100000, B00000000,
    B00000000, B00001100, B00110000, B00000000,
    B00000000, B00011000, B00011000, B00000000,
    B00000000, B00011000, B00011000, B00000000,
    B00000000, B00110000, B00001100, B00000000,
    B00000000, B00110000, B00001100, B00000000,
    B00000000, B01100000, B00000110, B00000000,
    B00000000, B01100000, B00000110, B00000000,
    B00000000, B11000000, B00000011, B00000000,
    B00000000, B11001001, B10010011, B00000000,
    B00000001, B10010001, B00110001, B10000000,
    B00000001, B10010010, B00100001, B10000000,
    B00000011, B00010010, B00100000, B11000000,
    B00000011, B00010001, B00100000, B11000000,
    B00000110, B00001000, B10010000, B01100000,
    B00000110, B00000100, B10011000, B01100000,
    B00001100, B00000100, B01001000, B00110000,
    B00001100, B00000100, B01001000, B00110000,
    B00011000, B00001100, B11011000, B00011000,
    B00011000, B00001000, B10010000, B00011000,
    B00110000, B00000000, B00000000, B00001100,
    B00100000, B11111111, B11111111, B00000100,
    B01100000, B01111111, B11111110, B00000110,
    B11000000, B00000000, B00000000, B00000011,
    B11000000, B00000000, B00000000, B00000011,
    B11111111, B11111111, B11111111, B11111111,
    B01111111, B11111111, B11111111, B11111110};

const unsigned char term_icon[] PROGMEM = {
    B00011100, B00000000,
    B00100010, B00000000,
    B00100010, B00000000,
    B00100010, B00000000,
    B00100010, B00000000,
    B00100010, B00000000,
    B00100010, B00000000,
    B00100011, B10000000,
    B00100010, B00000000,
    B00100010, B00000000,
    B00100010, B00000000,
    B00100011, B10000000,
    B00100010, B00000000,
    B00100010, B00000000,
    B00101010, B00000000,
    B00101011, B10000000,
    B00101010, B00000000,
    B00101010, B00000000,
    B00101010, B00000000,
    B00101010, B00000000,
    B00101011, B10000000,
    B00101010, B00000000,
    B00101010, B00000000,
    B00101010, B00000000,
    B01011101, B00000000,
    B10111110, B10000000,
    B10111110, B10000000,
    B10111110, B10000000,
    B10111110, B10000000,
    B10011100, B10000000,
    B01000001, B00000000,
    B00111110, B00000000};
const unsigned char wifi_icon[] PROGMEM = {
  B01010001,B01000000,
  B10100100,B10100000,
  B10101110,B10100000,
  B10101110,B10100000,
  B10100100,B10100000,
  B01010101,B01000000,
  B00000100,B00000000,
  B00000100,B00000000,
  B00000100, B00000000,
};
