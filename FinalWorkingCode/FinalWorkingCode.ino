// This is the complete working code (with more language options).
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

#include "SPIFFS.h"
#include "driver/i2s.h" 

#include "STT.h"
#include "TTT.h"
#include "TTS.h"
#include "OLED_Display.h"

// --- PINS ---
#define START_BUTTON_PIN   15
#define SELECT_BUTTON_PIN  13
#define DIRECTION_PIN      21

const char* ssid = "DESKTOP-SE00GAS 5587";
const char* password = "51215121";

// --- LANGUAGE CONFIGURATION ---
struct LanguagePair {
  String name1; 
  String dg_code1;  // Deepgram Code (e.g., "en")
  String tts_code1; // Google TTS Code (e.g., "en-IN")
  
  String name2; 
  String dg_code2; 
  String tts_code2; 
};

// Define your supported pairs here
LanguagePair languages[] = {
  // Indian Languages
  { "English", "en", "en-IN",  "Hindi",   "hi", "hi-IN" },
  { "English", "en", "en-IN",  "Bengali", "bn", "bn-IN" },
  { "English", "en", "en-IN",  "Tamil",   "ta", "ta-IN" },
  
  // International Languages
  { "English", "en", "en-US",  "Spanish", "es", "es-ES" },
  { "English", "en", "en-US",  "French",  "fr", "fr-FR" },
  { "English", "en", "en-US",  "German",  "de", "de-DE" },
  { "English", "en", "en-US",  "Japanese","ja", "ja-JP" },
  { "English", "en", "en-US",  "Korean",  "ko", "ko-KR" }
};

int currentLangPair = 0;
const int numLangPairs = 8; // Update this number to match the list above!
bool direction_1_to_2 = true; 

String original_text = "";
bool lastDirectionState = false;

void setup() {
  Serial.begin(115200);
  delay(1000);

  // IO Setup
  pinMode(START_BUTTON_PIN, INPUT_PULLUP);
  pinMode(SELECT_BUTTON_PIN, INPUT_PULLUP);
  pinMode(DIRECTION_PIN, INPUT_PULLUP);

  // Initialize OLED
  oledInit(); // Shows "Booting..."

  // Initialize SPIFFS
  Serial.println("Initializing SPIFFS...");
  if (!SPIFFS.begin(true)) {
    oledPrint("SPIFFS Failed!", 2, 20);
    while(1);
  }

  // WiFi Setup
  oledPrint("Connecting\nWiFi...", 2, 20);
  Serial.print("Connecting to Wi-Fi: ");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi connected!");
  
  // Initial Display Update
  lastDirectionState = digitalRead(DIRECTION_PIN);
  direction_1_to_2 = (lastDirectionState == HIGH); // Logic depending on your switch wiring
  
  // Show 2. Select language pair (Default)
  displayIdleState(languages[currentLangPair].name1, languages[currentLangPair].name2, direction_1_to_2);
}

void loop() {
  
  // --- 1. Check Direction Switch ---
  bool currentDirState = digitalRead(DIRECTION_PIN);
  if (currentDirState != lastDirectionState) {
    delay(50); // Debounce
    if (digitalRead(DIRECTION_PIN) == currentDirState) {
      lastDirectionState = currentDirState;
      direction_1_to_2 = (currentDirState == HIGH); 
      
      // Update Display
      displayIdleState(languages[currentLangPair].name1, languages[currentLangPair].name2, direction_1_to_2);
      Serial.println("Direction Changed");
    }
  }

  // --- 2. Check Select Button ---
  if (digitalRead(SELECT_BUTTON_PIN) == LOW) {
    delay(200); // Debounce
    currentLangPair++;
    if (currentLangPair >= numLangPairs) {
      currentLangPair = 0;
    }
    
    // Update Display
    displayIdleState(languages[currentLangPair].name1, languages[currentLangPair].name2, direction_1_to_2);
    Serial.printf("Language Pair Changed: %d\n", currentLangPair);
    
    // Wait for release
    while(digitalRead(SELECT_BUTTON_PIN) == LOW); 
  }

  // --- 3. Check Start Button (Record) ---
  if (digitalRead(START_BUTTON_PIN) == LOW) {
    
    // Define Source and Target variables
    String sourceName, targetName;
    String stt_code, translate_source_code, translate_target_code, tts_code;
    
    // Select codes based on direction
    if (direction_1_to_2) {
      sourceName = languages[currentLangPair].name1;
      stt_code   = languages[currentLangPair].dg_code1;  // Deepgram code for mic
      translate_source_code = languages[currentLangPair].dg_code1; // Source for Google Translate

      targetName = languages[currentLangPair].name2;
      translate_target_code = languages[currentLangPair].dg_code2; // Target for Google Translate
      tts_code   = languages[currentLangPair].tts_code2; // Google TTS code for speaker
    } else {
      sourceName = languages[currentLangPair].name2;
      stt_code   = languages[currentLangPair].dg_code2;
      translate_source_code = languages[currentLangPair].dg_code2;

      targetName = languages[currentLangPair].name1;
      translate_target_code = languages[currentLangPair].dg_code1;
      tts_code   = languages[currentLangPair].tts_code1;
    }

    // --- 4. Start Recording ---
    oledPrint("Listening...", 2, 25);
    Serial.println("\n[STEP 1] SPEECH TO TEXT");
    
    recordAudio(); // Blocks until button released
    
    oledPrint("Processing...", 2, 25);
    Serial.println("Recording Stopped. Sending to Deepgram...");
    
    // Reset text variable
    original_text = ""; 
    sendToDeepgram(original_text, stt_code);

    if (original_text.length() > 0) {
      Serial.println("Transcript: " + original_text);
      
      // --- 5. Text to Text ---
      Serial.println("[STEP 2] TEXT TO TEXT");
      oledPrint("Translating...", 2, 25);
      String translated_text = translateText(original_text, translate_source_code, translate_target_code);
      
      if (translated_text != "Error") {
        Serial.println("Translation: " + translated_text);
        
        // --- 6. Text to Speech ---
        Serial.println("[STEP 3] TEXT TO SPEECH");
        oledPrint("Speaking...", 2, 25);
        
        // --- 7. Playing Audio ---
        playTranslatedAudio(translated_text, tts_code);
        
      } else {
        oledPrint("Trans Error", 2, 25);
        delay(2000);
      }
    } else {
      oledPrint("No Speech", 2, 25);
      Serial.println("Deepgram returned empty text.");
      delay(2000);
    }

    // Return to Idle Menu
    displayIdleState(languages[currentLangPair].name1, languages[currentLangPair].name2, direction_1_to_2);
  }

  delay(50); // Small loop delay
}