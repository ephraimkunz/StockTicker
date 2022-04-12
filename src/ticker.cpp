#include <config_server.h>
#include <metrics.h>
#include <ticker.h>

#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>

// Globals
int ticker_prices_size = 0;
TickerPrice ticker_prices[MAX_NUM_TICKER_PRICES];

unsigned long ticker_last_fetch_ms = 0;
bool ticker_has_previous_error = false;

static const unsigned long FETCH_INTERVAL_MS = 1000 * 60;      // 60 seconds
static const unsigned long ERROR_FETCH_INTERVAL_MS = 1000 * 5; // Five seconds

unsigned long ticker_fetch_interval_ms(bool has_previous_error) {
  // If there was an error last time, try again sooner.
  if (has_previous_error) {
    return ERROR_FETCH_INTERVAL_MS;
  } else {
    return FETCH_INTERVAL_MS;
  }
}

// Setup request code
static std::unique_ptr<BearSSL::WiFiClientSecure>
    client(new BearSSL::WiFiClientSecure);

void ticker_init() {
  client->setInsecure();
  config_init_default_ticker_symbols();
}

static void update_price_setting_needs_draw(TickerPrice *ticker,
                                            std::optional<float> new_price) {
  if (ticker->price != new_price) {
    ticker->price = new_price;
    ticker->needs_draw = true;
  }
}

void ticker_update_prices() {
  ticker_has_previous_error = false;
  DynamicJsonDocument doc(2048);

  bool needs_draw_for_size_change = false;
  int num_ticker_symbols_to_fetch = config_num_ticker_symbols();
  if (ticker_prices_size != num_ticker_symbols_to_fetch) {
    ticker_prices_size = num_ticker_symbols_to_fetch;
    needs_draw_for_size_change = true;
  }

  metrics_print("ticker_update_prices");

  for (int i = 0; i < num_ticker_symbols_to_fetch; i++) {
    ticker_prices[i].needs_draw = needs_draw_for_size_change ? true : false;
    int cmp = strncmp(ticker_prices[i].symbol, config_ticker_symbol_at_index(i),
                      MAX_SYMBOL_LENGTH);

    if (cmp != 0) {
      // A different stock now occupies this slot.
      strncpy(ticker_prices[i].symbol, config_ticker_symbol_at_index(i),
              MAX_SYMBOL_LENGTH);
      ticker_prices[i].needs_draw = true;
    }
  }

  for (int i = 0; i < num_ticker_symbols_to_fetch; i++) {
    metrics_print("before request");

    const String serverName =
        "https://query1.finance.yahoo.com/v8/finance/chart/";
    const String intervalQuery = "?interval=1d";

    HTTPClient http;
    String request =
        serverName + config_ticker_symbol_at_index(i) + intervalQuery;
    Serial.print("Starting request: \"");
    Serial.print(request);
    Serial.println("\"");

    metrics_print("before http.begin");

    http.begin(*client, request);
    int httpResponseCode = http.GET();

    Serial.print("Ending request: \"");
    Serial.print(request);
    Serial.println("\"");

    metrics_print("after http.begin");

    if (httpResponseCode > 0) {
      if (httpResponseCode == 200) {
        Serial.print("HTTP Response Code: ");
        Serial.println(httpResponseCode);
        WiFiClient &stream = http.getStream();

        DeserializationError error = deserializeJson(doc, stream);
        if (error) {
          Serial.print(F("deserializeJson() failed: "));
          Serial.println(error.f_str());
          Serial.print("Document capacity allocated: ");
          Serial.println(doc.capacity());
          ticker_has_previous_error = true;
          update_price_setting_needs_draw(&ticker_prices[i], std::nullopt);
        } else {
          JsonObject chart_result_0 = doc["chart"]["result"][0];
          JsonObject chart_result_0_meta = chart_result_0["meta"];
          float market_price = chart_result_0_meta["regularMarketPrice"];
          update_price_setting_needs_draw(&ticker_prices[i], market_price);
        }
      } else {
        Serial.print("Non 200 response to HTTP request: ");
        Serial.println(request);
        ticker_has_previous_error = true;
        update_price_setting_needs_draw(&ticker_prices[i], std::nullopt);
      }
    } else {
      Serial.print("Error in HTTP request: ");
      Serial.println(httpResponseCode);
      ticker_has_previous_error = true;
      update_price_setting_needs_draw(&ticker_prices[i], std::nullopt);
    }
    http.end();
  }
}