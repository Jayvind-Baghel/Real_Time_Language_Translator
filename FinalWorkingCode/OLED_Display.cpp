#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "OLED_Display.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

void oledInit(){
  Serial.println("Initializing OLED...");
  Wire.begin(22,23); // SDA, SCL
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
    Serial.println(F("SSD1306 allocation failed"));
    while(1);
  }
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  oledPrint("Booting...", 2, 25);
  delay(1000);
}

void oledPrint(String text, int size, int y) {
  display.clearDisplay();
  display.setTextSize(size);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, y);
  display.println(text);
  display.display();
}

// New function to draw the main menu
void displayIdleState(String lang1, String lang2, bool direction) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  
  // Draw Header
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Select Language:");
  display.drawLine(0, 10, 128, 10, SSD1306_WHITE);

  // Draw Languages and Arrow
  display.setTextSize(1);
  display.setCursor(0, 20);
  
  if(direction) {
    // Lang 1 -> Lang 2
    display.print(lang1); 
    display.print(" -> "); 
    display.println(lang2);
  } else {
    // Lang 2 -> Lang 1
    display.print(lang2); 
    display.print(" -> "); 
    display.println(lang1);
  }

  // Draw Footer Instruction
  display.setTextSize(1);
  display.setCursor(0, 45);
  display.println("Hold Start to Speak");
  
  display.display();
}