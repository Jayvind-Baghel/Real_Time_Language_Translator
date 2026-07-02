#ifndef __TTS_H__
#define __TTS_H__

#include "TTS.h"

#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "SPIFFS.h"

#include "driver/i2s.h"
#include "minimp3.h"       

#include "OLED_Display.h"

extern const char* tts_mp3_path;

void installSpeakerDriver(int sampleRate);
void uninstallI2SDriver_spk();
String urlEncode_tts(String str);

void playMP3FromSPIFFS(const char* filePath);
void downloadAndPlayAudio(const char* text, const char* language);
void playTranslatedAudio(String text, String language);


#endif 