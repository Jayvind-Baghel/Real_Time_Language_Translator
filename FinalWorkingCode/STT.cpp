#include "STT.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "SPIFFS.h"
#include "driver/i2s.h" 
#include "OLED_Display.h"

#define I2S_PORT_NUM    I2S_NUM_0
#define I2S_MIC_BCLK    (12)
#define I2S_MIC_WS      (14)
#define I2S_MIC_SD      (5)

#define START_BUTTON_PIN   15

const char* stt_wav_path = "/stt_recording.wav";
const int headerSize = 44;

// Deepgram Key
const char* deepgramApiKey = "f46583d458c5f1687814fca8f49869f8955f4138";

void installMicDriver() {
  Serial.println("[Mic] Initializing I2S Port 0 for recording...");
  i2s_config_t i2s_config_mic = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = 16000,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT, // Check your mic (LEFT or RIGHT)
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 64,
    .use_apll = false
  };
  i2s_driver_install(I2S_PORT_NUM, &i2s_config_mic, 0, NULL);
  i2s_pin_config_t pin_config_mic = {
    .bck_io_num = I2S_MIC_BCLK,
    .ws_io_num = I2S_MIC_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_MIC_SD
  };
  i2s_set_pin(I2S_PORT_NUM, &pin_config_mic);
}

void uninstallI2SDriver_mic() {
  i2s_driver_uninstall(I2S_PORT_NUM);
  delay(10); 
}

void createWavHeader(byte* header, int wavDataSize) {
  header[0] = 'R'; header[1] = 'I'; header[2] = 'F'; header[3] = 'F';
  unsigned int fileSize = wavDataSize + headerSize - 8;
  header[4] = (byte)(fileSize & 0xFF);
  header[5] = (byte)((fileSize >> 8) & 0xFF);
  header[6] = (byte)((fileSize >> 16) & 0xFF);
  header[7] = (byte)((fileSize >> 24) & 0xFF);
  header[8] = 'W'; header[9] = 'A'; header[10] = 'V'; header[11] = 'E';
  header[12] = 'f'; header[13] = 'm'; header[14] = 't'; header[15] = ' ';
  header[16] = 16; header[17] = 0; header[18] = 0; header[19] = 0;
  header[20] = 1; header[21] = 0;
  header[22] = 1; header[23] = 0; 
  header[24] = 0x80; header[25] = 0x3E; header[26] = 0x00; header[27] = 0x00; 
  header[28] = 0x00; header[29] = 0x7D; header[30] = 0x00; header[31] = 0x00; 
  header[32] = 2; header[33] = 0; 
  header[34] = 16; header[35] = 0; 
  header[36] = 'd'; header[37] = 'a'; header[38] = 't'; header[39] = 'a';
  header[40] = (byte)(wavDataSize & 0xFF);
  header[41] = (byte)((wavDataSize >> 8) & 0xFF);
  header[42] = (byte)((wavDataSize >> 16) & 0xFF);
  header[43] = (byte)((wavDataSize >> 24) & 0xFF);
}

void sendToDeepgram(String &originalText, String langCode) {
  if (!SPIFFS.exists(stt_wav_path)) {
    Serial.println("No recording found.");
    return;
  }

  // --- FIX 2: Reconnect Wi-Fi if dropped ---
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi dropped! Reconnecting...");
    while (WiFi.status() != WL_CONNECTED) {
      delay(100);
      Serial.print(".");
    }
    Serial.println("\nReconnected.");
  }

  Serial.printf("Sending to Deepgram (Lang: %s)...\n", langCode.c_str());
  
  File file1 = SPIFFS.open(stt_wav_path, FILE_READ);
  if (!file1) return;

  // Check file size (prevent sending empty files)
  if (file1.size() < 1000) {
     Serial.println("Recording too short/empty. Skipping upload.");
     file1.close();
     return;
  }

  HTTPClient http;
  String url = "https://api.deepgram.com/v1/listen?model=nova-2&smart_format=true&language=" + langCode;
  
  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(10000); // Increase timeout to 10 seconds

  if(http.begin(client, url)) {
      http.addHeader("Authorization", String("Token ") + deepgramApiKey);
      http.addHeader("Content-Type", "audio/wav");
      
      int httpResponseCode = http.sendRequest("POST", &file1, file1.size());
      
      if (httpResponseCode > 0) {
        String response = http.getString();
        int transcriptPos = response.indexOf("\"transcript\":");
        if (transcriptPos > 0) {
          int startQuote = response.indexOf("\"", transcriptPos + 13);
          int endQuote = response.indexOf("\"", startQuote + 1);
          String transcript = response.substring(startQuote + 1, endQuote);
          originalText = transcript;
        }
      } else {
        Serial.printf("Deepgram Error: %s\n", http.errorToString(httpResponseCode).c_str());
      }
      http.end();
  } else {
      Serial.println("Unable to connect to Deepgram");
  }
  
  file1.close();
}

// --- Updated recordAudio ---
void recordAudio(){
  installMicDriver();
  
  // Delete old file to ensure fresh start
  if(SPIFFS.exists(stt_wav_path)) SPIFFS.remove(stt_wav_path);

  File wavFile = SPIFFS.open(stt_wav_path, "w");
  if (!wavFile) {
    Serial.println("Failed to create recording file");
    uninstallI2SDriver_mic();
    return;
  }

  byte header[headerSize];
  createWavHeader(header, 0);
  wavFile.write(header, headerSize);
  
  int16_t sample_buffer[512];
  
  // --- RECORDING LOOP ---
  while (digitalRead(START_BUTTON_PIN) == LOW) { 
    size_t bytes_read = 0;
    i2s_read(I2S_NUM_0, &sample_buffer, 512 * 2, &bytes_read, portMAX_DELAY);
    if (bytes_read > 0) {
      wavFile.write((byte*)sample_buffer, bytes_read);
    }
    // --- FIX 1: Let Wi-Fi Stack Run ---
    delay(1); 
  }

  // Fix header
  unsigned long fileSize = wavFile.size();
  createWavHeader(header, fileSize - headerSize);
  wavFile.seek(0);
  wavFile.write(header, headerSize);
  wavFile.close();
  
  uninstallI2SDriver_mic();
  Serial.printf("Recording size: %u bytes\n", fileSize); // Debug line
}