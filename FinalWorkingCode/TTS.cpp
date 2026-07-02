#include "TTS.h"

#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "SPIFFS.h"

#include "driver/i2s.h"
#include "minimp3.h"       

#include "OLED_Display.h"


// --- I2S Speaker (Output on Port 1) ---
#define I2S_PORT_NUM    I2S_NUM_0
#define I2S_SPK_BCLK    (26)
#define I2S_SPK_LRC     (27)
#define I2S_SPK_DIN     (25)


#define MP3_INPUT_BUFFER_SIZE  1024 
#define MAX_PCM_SAMPLES (1152 * 2) // Max samples * max channels
#define STEREO_BUFFER_SIZE (MAX_PCM_SAMPLES * 2) // * 2 again for Stereo

static uint8_t mp3_input_buffer[MP3_INPUT_BUFFER_SIZE];
static short pcm_output_buffer[STEREO_BUFFER_SIZE]; // <-- Increased buffer size

const char* tts_mp3_path = "/tts_audio.mp3";
const int headerSize = 44;

// --- minimp3 Decoder Objects ---
static mp3dec_t mp3d;
static mp3dec_frame_info_t info;


void installSpeakerDriver(int sampleRate) {
  Serial.printf("[Speaker] Initializing I2S Port 0 for playback (%d Hz, 2Ch-Forced)...\n", sampleRate);
  
  i2s_config_t i2s_config_spk = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = (uint32_t)sampleRate,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT, // <-- FIX: Always use Stereo
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 1024,
    .use_apll = true, // <-- FIX: Use high-precision clock
    .tx_desc_auto_clear = true
  };
  
  i2s_driver_install(I2S_PORT_NUM, &i2s_config_spk, 0, NULL);
  
  i2s_pin_config_t pin_config_spk = {
    .bck_io_num = I2S_SPK_BCLK,
    .ws_io_num = I2S_SPK_LRC,
    .data_out_num = I2S_SPK_DIN,
    .data_in_num = I2S_PIN_NO_CHANGE
  };
  
  i2s_set_pin(I2S_PORT_NUM, &pin_config_spk);
  i2s_zero_dma_buffer(I2S_PORT_NUM);
  Serial.println("[Speaker] I2S Port 0 initialized successfully.");
}

void uninstallI2SDriver_spk() {
  Serial.println("[I2S] De-initializing I2S Port 0...");
  i2s_driver_uninstall(I2S_PORT_NUM);
  delay(10); // Give time for the driver to release
}

String urlEncode_tts(String str) {
  String encodedString = "";
  char c;
  char code0;
  char code1;
  for (int i = 0; i < str.length(); i++) {
    c = str.charAt(i);
    if (c == ' ') {
      encodedString += "%20";
    } else if (isalnum(c)) {
      encodedString += c;
    } else {
      code1 = (c & 0xf) + '0';
      if ((c & 0xf) > 9) {
        code1 = (c & 0xf) - 10 + 'A';
      }
      c = (c >> 4) & 0xf;
      code0 = c + '0';
      if (c > 9) {
        code0 = c - 10 + 'A';
      }
      encodedString += '%';
      encodedString += code0;
      encodedString += code1;
    }
  }
  return encodedString;
}

void playMP3FromSPIFFS(const char* filePath) {

  File f = SPIFFS.open(filePath, FILE_READ);
  if (!f) {
    Serial.println("Failed to open MP3 file");
    uninstallI2SDriver_spk();
    return;
  }

  Serial.printf("Playing MP3: %s\n", filePath);
  
  mp3dec_init(&mp3d);
  
  int sampleRate = 0;
  int channels = 0;
  bool isFirstFrame = true;

  int bytesLeft = 0;
  uint8_t *readPtr = mp3_input_buffer;

  while (f.available() > 0) {
    int bytesToRead = MP3_INPUT_BUFFER_SIZE - bytesLeft;
    int bytes_read_from_file = f.read(readPtr, bytesToRead);
    if (bytes_read_from_file == 0) break; 

    int bytesToDecode = bytesLeft + bytes_read_from_file;
    uint8_t *decodePtr = mp3_input_buffer;
    
    while(bytesToDecode > 0) {
      // Decode one frame into the first half of the buffer
      int pcm_samples_per_channel = mp3dec_decode_frame(&mp3d, decodePtr, bytesToDecode, pcm_output_buffer, &info);
      
      if (pcm_samples_per_channel > 0) {
        if (isFirstFrame) {
          sampleRate = info.hz;
          channels = info.channels;
          // Install speaker driver with the correct sample rate
          installSpeakerDriver(sampleRate);
          isFirstFrame = false;
        }

        size_t bytes_to_write = 0;
        short* pcm_data_ptr = pcm_output_buffer;

        if (channels == 1) {
          
          for (int i = pcm_samples_per_channel - 1; i >= 0; i--) {
            short mono_sample = pcm_output_buffer[i];
            pcm_output_buffer[i*2]   = mono_sample; // Left Channel
            pcm_output_buffer[i*2+1] = mono_sample; // Right Channel
          }
          bytes_to_write = pcm_samples_per_channel * 2 * sizeof(short); // Now stereo
        } 
        else {
          
          bytes_to_write = pcm_samples_per_channel * channels * sizeof(short);
        }
       
        size_t bytes_written;
        i2s_write(I2S_PORT_NUM, pcm_output_buffer, bytes_to_write, &bytes_written, portMAX_DELAY);
      }

      int bytes_decoded = info.frame_bytes;
      if (bytes_decoded > 0) {
        bytesToDecode -= bytes_decoded;
        decodePtr += bytes_decoded;
      } else {
        break; 
      }
    }

    bytesLeft = bytesToDecode;
    memmove(mp3_input_buffer, decodePtr, bytesLeft);
    readPtr = mp3_input_buffer + bytesLeft;
  }

  f.close();
  
  if (!isFirstFrame) {
    uninstallI2SDriver_spk();
  }
  
  Serial.println("Playback finished.");
  uninstallI2SDriver_spk();
}

void downloadAndPlayAudio(const char* text, const char* language) {
  
  String encodedText = urlEncode_tts(text);
  String url = "http://translate.google.com/translate_tts?ie=UTF-8&q=";
  url += encodedText;
  url += "&tl=";
  url += String(language);
  url += "&client=tw-ob";

  Serial.println("-------------------------");
  Serial.printf("Speaking: '%s'\n", text);
  Serial.println("Downloading audio from Google...");

  HTTPClient http;
  http.begin(url); 
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS); 

  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    Serial.println("Connected. Saving to SPIFFS...");
    
    File f = SPIFFS.open(tts_mp3_path, "w");
    if (!f) {
      Serial.println("Failed to open file for writing");
      http.end();
      return;
    }

    http.writeToStream(&f);
    f.close(); 
    Serial.println("Download complete. Playing audio...");
    http.end(); 

    // Play the downloaded audio using the new decoder
    playMP3FromSPIFFS(tts_mp3_path);
    
  } else {
    Serial.printf("HTTP connection failed! Error code: %d\n", httpCode);
  }
  
  http.end();
}

void playTranslatedAudio(String text, String language) {
  Serial.println(">>> Speaking... <<<");
  
  int maxLen = 200; 
  for (int i = 0; i < text.length(); i += maxLen) {
    String chunk = text.substring(i, i + maxLen);
    downloadAndPlayAudio(chunk.c_str(), language.c_str());
    delay(200); // Small pause between chunks
  }
}
