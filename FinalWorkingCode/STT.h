#ifndef __STT_H__
#define __STT_H__

#include <Arduino.h>

extern const char* stt_wav_path;

void installMicDriver();
void uninstallI2SDriver_mic();
void createWavHeader(byte* header, int wavDataSize);
void recordAudio();

// Changed to accept language code
void sendToDeepgram(String &originalText, String langCode); 

#endif