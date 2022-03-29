#include <ESP8266WiFi.h>
#include <screen.h>
#include <config_server.h>
#include <ticker.h>

// Setup wifi
const char *ssid = "1450Hill";
const char *password = "yellowhippo095";

void wifi_init() {
  WiFi.begin(ssid, password);
  while (!WiFi.isConnected()) {
    Serial.println(".");
    delay(500);
  }
}

void setup() {
  Serial.begin(115200);

  screen_init();
  wifi_init();
  config_server_init();
  ticker_init();

  screen_write_splash_screen();
}

void loop() {
  if ((WiFi.status() == WL_CONNECTED) &&
      (config_server_changed_configuration ||
       ((ticker_last_fetch_ms + ticker_fetch_interval_ms(ticker_has_previous_error)) <= millis()))) {
    config_server_changed_configuration = false;
    ticker_update_prices();
    screen_clamp_current_screen();
    screen_write_stock_screen_if_needed();
    ticker_last_fetch_ms = millis();
  }

  screen_handle_touch();

  delay(100);
}