#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include "OLED_Display.h"

String T2T_SCRIPT_URL = "https://script.google.com/macros/s/AKfycbwjZQBOd3hO3sucYWSsAQgXpRCi_lXBN3lKf0Ls-_arekKNeW-WRWRa42V8n0-vuXlo/exec";

String urlEncode_ttt(String str) {
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

String translateText(String text, String sourceLang, String targetLang) {
  oledPrint("Translating...", 2, 20);
  Serial.printf("Translating '%s' from %s to %s\n", text.c_str(), sourceLang.c_str(), targetLang.c_str());
  
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  
  String url = T2T_SCRIPT_URL;
  url += "?text=" + urlEncode_ttt(text);
  url += "&source=" + sourceLang;
  url += "&target=" + targetLang;

  // Serial.println("Making translation request:");
  
  if (http.begin(client, url)) {
    int httpCode = http.GET();
    String translatedText = "";

    if (httpCode > 0 && (httpCode == HTTP_CODE_MOVED_PERMANENTLY || httpCode == 302)) {
      String newUrl = http.getLocation();
      http.end(); 
      http.begin(client, newUrl);
      httpCode = http.GET();
    }

    if (httpCode == HTTP_CODE_OK) {
      translatedText = http.getString();
    } 
    else {
      Serial.printf("HTTP request failed! Error: %s\n", http.errorToString(httpCode).c_str());
      translatedText = "Error";
    }
    http.end();
    return translatedText;
  } 
  else {
    Serial.println("Failed to begin HTTP connection.");
    return "Error";
  }
}
