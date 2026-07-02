#ifndef __OLED_H__
#define __OLED_H__

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

void oledInit();
void oledPrint(String text, int size, int y);
// New function to show the language menu
void displayIdleState(String lang1, String lang2, bool direction); 

#endif