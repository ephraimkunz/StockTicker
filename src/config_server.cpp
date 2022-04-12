#include <config_server.h>

#include <ESPAsyncWebServer.h>

// Externs
bool config_server_changed_configuration = false;

// The number of active symbols in config_ticker_symbols_to_fetch.
int config_num_ticker_symbols_to_fetch = 0;

// Symbols the user has specified should be displayed. The number of active
// items in this array is config_num_ticker_symbols_to_fetch.
// The total space is [MAX_NUM_TICKER_PRICES][MAX_SYMBOL_LENGTH].
// See https://arduino-esp8266.readthedocs.io/en/latest/faq/a02-my-esp-crashes.html.
char *config_ticker_symbols_to_fetch;

static AsyncWebServer server(80);

static const char PARAM_INPUT_1[] PROGMEM = "input1";
static const char INDEX[] PROGMEM =
    "<!DOCTYPE HTML><html><head><title>ESP8266 Stock Ticker</title><meta "
    "name=\"viewport\" content=\"width=device-width, "
    "initial-scale=1\"></head><body><p>Enter a single ticker symbol or "
    "multiple pipe-separated symbols</p>"
    "<form action=\"/get\">Ticker Symbol(s): "
    "<input type=\"text\" name=\"input1\"><input type=\"submit\" "
    "value=\"Submit\"></form><br><a "
    "href=\"https://github.com/ephraimkunz/StockTicker\">ESP8266StockTicker</"
    "a></body></html>";

int config_num_ticker_symbols() { return config_num_ticker_symbols_to_fetch; }

char *config_ticker_symbol_at_index(int index) {
  return &config_ticker_symbols_to_fetch[index * MAX_SYMBOL_LENGTH];
}

void config_init_default_ticker_symbols() {
  config_num_ticker_symbols_to_fetch = 2;

  strncpy(config_ticker_symbol_at_index(0), "AAPL", MAX_SYMBOL_LENGTH - 1);
  strncpy(config_ticker_symbol_at_index(1), "GLD", MAX_SYMBOL_LENGTH - 1);

  // Start with a changed config flag set, so we immediately fetch the prices
  // for these.
  config_server_changed_configuration = true;
}

void config_server_init() {
  config_ticker_symbols_to_fetch =
      (char *)malloc(MAX_NUM_TICKER_PRICES * MAX_SYMBOL_LENGTH);
  Serial.print("Configure at: ");
  Serial.println(WiFi.localIP());

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", INDEX);
  });
  server.on("/get", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam(PARAM_INPUT_1)) {
      config_num_ticker_symbols_to_fetch = 0;

      char *tickerSymbolString =
          (char *)request->getParam(PARAM_INPUT_1)->value().c_str();

      Serial.print("Configuration update with new ticker symbol string: ");
      Serial.println(tickerSymbolString);

      const char separator[2] = "|";

      char *token = strtok(tickerSymbolString, separator);
      while (token &&
             config_num_ticker_symbols_to_fetch < MAX_NUM_TICKER_PRICES) {
        // Strip leading spaces.
        while (isspace(*token)) {
          token++;
        }

        for (int i = 0; i < MAX_SYMBOL_LENGTH; i++) {
          // Strip trailing spaces.
          if (isspace(token[i])) {
            token[i] = '\0';
          }

          config_ticker_symbols_to_fetch[config_num_ticker_symbols_to_fetch *
                                             MAX_SYMBOL_LENGTH +
                                         i] = toupper(token[i]);

          if (token[i] == '\0') {
            break;
          }
        }

        config_num_ticker_symbols_to_fetch++;

        // Next token.
        token = strtok(NULL, separator);
      }
    }

    Serial.println("Setting config changed flag");
    config_server_changed_configuration = true;
    request->redirect("/");
  });

  server.onNotFound([](AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Not found");
  });

  server.begin();
}