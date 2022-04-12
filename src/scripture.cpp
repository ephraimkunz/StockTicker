#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <scripture.h>

// Global scripture.
Scripture scripture;

// Setup HTTP request code
static std::unique_ptr<BearSSL::WiFiClientSecure>
    client(new BearSSL::WiFiClientSecure);

void scripture_init() { client->setInsecure(); }
void scripture_update() {
  const String endpoint = "https://random-verse.shuttleapp.rs";

  HTTPClient http;
  http.begin(*client, endpoint);
  int httpResponseCode = http.GET();

  if (httpResponseCode > 0) {
    if (httpResponseCode == 200) {
      Serial.print("HTTP Response Code: ");
      Serial.println(httpResponseCode);
      WiFiClient &stream = http.getStream();

      DynamicJsonDocument doc(1024);

      DeserializationError error = deserializeJson(doc, stream);
      if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        Serial.print("Document capacity allocated: ");
        Serial.println(doc.capacity());
      } else {
        const char *verse_title = doc["verse_short_title"];
        strncpy(scripture.verse_title, verse_title, MAX_VERSE_TITLE_LENGTH - 1);
        const char *text = doc["scripture_text"];
        strncpy(scripture.verse_text, text, MAX_VERSE_TEXT_LENGTH - 1);
      }
    } else {
      Serial.print("Non 200 response to HTTP request: ");
      Serial.println(httpResponseCode);
    }
  } else {
    Serial.print("Error in HTTP request: ");
    Serial.println(httpResponseCode);
  }
  http.end();
}