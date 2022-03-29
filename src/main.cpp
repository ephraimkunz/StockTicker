#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>
#include <Wire.h>
#include <XPT2046_Touchscreen.h>

#define MAX_NUM_TICKER_PRICES 10
#define MAX_SYMBOL_LENGTH 10

// Ticker fetching and display globals
unsigned long last_fetch = 0;
bool previousError = false;
const unsigned long FETCH_INTERVAL = 1000 * 60;  // 60 seconds
const unsigned long ERROR_FETCH_INTERVAL = 1000 * 5; // Five seconds

unsigned long fetch_interval(bool previousError) {
  // If there was an error last time, try again sooner.
  if (previousError) {
    return ERROR_FETCH_INTERVAL;
  } else {
    return FETCH_INTERVAL;
  }
}

struct TickerPrice {
  char symbol[MAX_SYMBOL_LENGTH];
  std::optional<float> price;
  bool needs_draw;
};

int ticker_prices_size = 0;
TickerPrice ticker_prices[MAX_NUM_TICKER_PRICES];

// As received from the server, but not yet visible to the user.
int num_ticker_symbols_to_fetch = 0;
char ticker_symbols_to_fetch[MAX_NUM_TICKER_PRICES][MAX_SYMBOL_LENGTH];

// Which page index the user is currently on.
int current_screen = 0;

// Setup touchscreen and display
#define TFT_CS D0
#define TFT_DC D8
#define TFT_RST -1
#define TS_CS D3

// This is calibration data for the raw touch data to the screen coordinates
#define TS_MINX 250
#define TS_MINY 350
#define TS_MAXX 3700
#define TS_MAXY 3600

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);
XPT2046_Touchscreen ts(TS_CS);

struct Rect {
  int x;
  int y;
  int w;
  int h;
};

const int NAVIGATE_BUTTON_RADIUS = 20;
const int NAVIGATE_BUTTON_PADDING = 5;

bool shows_left_navigate_button() { return current_screen > 0; }

bool shows_right_navigate_button() {
  return current_screen < (ticker_prices_size - 1);
}

Rect left_navigate_bounding_box() {
  return Rect{NAVIGATE_BUTTON_PADDING,
              (tft.height() / 2) - (NAVIGATE_BUTTON_RADIUS / 2),
              NAVIGATE_BUTTON_RADIUS * 2, NAVIGATE_BUTTON_RADIUS * 2};
}

Rect right_navigate_bounding_box() {
  return Rect{tft.width() - NAVIGATE_BUTTON_RADIUS - NAVIGATE_BUTTON_RADIUS -
                  NAVIGATE_BUTTON_PADDING,
              (tft.height() / 2) - (NAVIGATE_BUTTON_RADIUS / 2),
              NAVIGATE_BUTTON_RADIUS * 2, NAVIGATE_BUTTON_RADIUS * 2};
}

void write_splash_screen() {
  tft.fillScreen(ILI9341_BLACK);
  tft.setCursor(0, 0);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(3);
  tft.println("Stock Ticker");

  tft.setTextSize(2);
  tft.println("By Ephraim Kunz");

  tft.println("");

  if (WiFi.isConnected()) {
    tft.print("Configure at ");
    tft.println(WiFi.localIP());
  } else {
    tft.println("Connecting to WiFi...");
  }
}

void write_stock_screen() {
  if (current_screen == -1) {
    write_splash_screen();
    return;
  }

  TickerPrice ticker_price = ticker_prices[current_screen];

  if (!ticker_price.needs_draw) {
    return;
  }

  tft.fillScreen(ILI9341_BLACK);
  tft.setTextColor(ILI9341_WHITE);

  tft.setCursor(60, 20);
  tft.setTextSize(5);
  tft.print(ticker_price.symbol);

  tft.setCursor(60, 80);
  if (auto price = ticker_price.price) {
    tft.setTextSize(6);
    tft.println(*price);
  } else {
    tft.setTextSize(2);
    tft.println("Unable to fetch the price for this symbol");
  }

  if (shows_right_navigate_button()) {
    Rect r = right_navigate_bounding_box();
    tft.fillCircle(r.x + r.w / 2, r.y + r.h / 2, NAVIGATE_BUTTON_RADIUS,
                   ILI9341_GREEN);
    tft.setCursor(r.x + (r.w / 2) - 6, r.y + (r.h / 2) - 10);
    tft.setTextColor(ILI9341_BLACK);
    tft.setTextSize(3);
    tft.print(">");
  }

  if (shows_left_navigate_button()) {
    Rect r = left_navigate_bounding_box();
    tft.fillCircle(r.x + r.w / 2, r.y + r.h / 2, NAVIGATE_BUTTON_RADIUS,
                   ILI9341_GREEN);
    tft.setCursor(r.x + (r.w / 2) - 10, r.y + (r.h / 2) - 10);
    tft.setTextColor(ILI9341_BLACK);
    tft.setTextSize(3);
    tft.print("<");
  }

  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(1);
  tft.setCursor(20, tft.height() - 20);
  tft.print(WiFi.localIP());
}

void init_tft() {
  tft.begin();
  tft.setRotation(1);
  write_splash_screen();
}

void init_ts() {
  ts.begin();
  ts.setRotation(1);
}

// Setup config web server
bool changed_configuration = false;
AsyncWebServer server(80);

String serverName = "https://query1.finance.yahoo.com/v8/finance/chart/";
String intervalQuery = "?interval=1d";
const char *PARAM_INPUT_1 = "input1";
const char INDEX[] =
    "<!DOCTYPE HTML><html><head><title>ESP8266 Stock Ticker</title><meta "
    "name=\"viewport\" content=\"width=device-width, "
    "initial-scale=1\"></head><body><p>Enter a single ticker symbol or "
    "multiple pipe-separated symbols</p>"
    "<form action=\"/get\">Ticker Symbol(s): "
    "<input type=\"text\" name=\"input1\"><input type=\"submit\" "
    "value=\"Submit\"></form><br><a "
    "href=\"https://github.com/Patrick-E-Rankin/"
    "ESP32StockTicker\">ESP8266StockTicker</a></body></html>";

void init_config_server() {
  Serial.print("Configure at: ");
  Serial.println(WiFi.localIP());

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", INDEX);
  });
  server.on("/get", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam(PARAM_INPUT_1)) {
      num_ticker_symbols_to_fetch = 0;

      char *tickerSymbolString =
          (char *)request->getParam(PARAM_INPUT_1)->value().c_str();

      Serial.print("Configuration update with new ticker symbol string: ");
      Serial.println(tickerSymbolString);

      const char separator[2] = "|";

      char *token = strtok(tickerSymbolString, separator);
      while (token && num_ticker_symbols_to_fetch < MAX_NUM_TICKER_PRICES) {
        // Strip leading spaces.
        while (isspace(*token)) {
          token++;
        }

        for (int i = 0; i < MAX_SYMBOL_LENGTH; i++) {
          // Strip trailing spaces.
          if (isspace(token[i])) {
            token[i] = '\0';
          }

          ticker_symbols_to_fetch[num_ticker_symbols_to_fetch][i] =
              toupper(token[i]);

          if (token[i] == '\0') {
            break;
          }
        }

        num_ticker_symbols_to_fetch ++;

        // Next token.
        token = strtok(NULL, separator);
      }
    }

    changed_configuration = true;
    request->redirect("/");
  });

  server.onNotFound([](AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Not found");
  });

  server.begin();
}

// Setup wifi
const char *ssid = "1450Hill";
const char *password = "yellowhippo095";

void init_wifi() {
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.println(".");
    delay(500);
  }
}

// Setup HTTP request code
std::unique_ptr<BearSSL::WiFiClientSecure>
    client(new BearSSL::WiFiClientSecure);

void init_http_requests() { client->setInsecure(); }

// Utility functions

void init_default_ticker_symbols() {
  num_ticker_symbols_to_fetch = 2;

  strncpy(ticker_symbols_to_fetch[0], "AAPL", MAX_SYMBOL_LENGTH);
  strncpy(ticker_symbols_to_fetch[1], "GLD", MAX_SYMBOL_LENGTH);

  // Start with a changed config flag set, so we immediately fetch the prices
  // for these.
  changed_configuration = true;
}

void update_price_setting_needs_draw(TickerPrice *ticker, std::optional<float> new_price) {
  if (ticker->price != new_price) {
    ticker->price = new_price;
    ticker->needs_draw = true;
  }
}

void update_ticker_prices() {
  previousError = false;
  DynamicJsonDocument doc(2048);

  bool needs_draw_for_size_change = false;
  if (ticker_prices_size != num_ticker_symbols_to_fetch) {
    ticker_prices_size = num_ticker_symbols_to_fetch;
    needs_draw_for_size_change = true;
  }

  for (int i = 0; i < num_ticker_symbols_to_fetch; i++) {
    ticker_prices[i].needs_draw = needs_draw_for_size_change ? true : false;
    int cmp = strncmp(ticker_prices[i].symbol, ticker_symbols_to_fetch[i],
            MAX_SYMBOL_LENGTH);

    if (cmp != 0) {
      // A different stock now occupies this slot.
      strncpy(ticker_prices[i].symbol, ticker_symbols_to_fetch[i],
            MAX_SYMBOL_LENGTH);
      ticker_prices[i].needs_draw = true;
    }
    
    HTTPClient http;
    String request = serverName + ticker_symbols_to_fetch[i] + intervalQuery;
    http.begin(*client, request);
    int httpResponseCode = http.GET();

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
          previousError = true;
          update_price_setting_needs_draw(&ticker_prices[i], std::nullopt);
        } else {
          JsonObject chart_result_0 = doc["chart"]["result"][0];
          JsonObject chart_result_0_meta = chart_result_0["meta"];
          float market_price = chart_result_0_meta["regularMarketPrice"];
          update_price_setting_needs_draw(&ticker_prices[i], market_price);
        }
      } else {
        Serial.print("Error on HTTP request: ");
        Serial.println(request);
        previousError = true;
        update_price_setting_needs_draw(&ticker_prices[i], std::nullopt);
      }
    } else {
      Serial.print("Error Code: ");
      Serial.println(httpResponseCode);
      previousError = true;
      update_price_setting_needs_draw(&ticker_prices[i], std::nullopt);
    }
    http.end();
  }
}

void clamp_current_screen() {
  if (current_screen >= ticker_prices_size) {
    current_screen = 0;
  }

  if (ticker_prices_size == 0) {
    current_screen = -1;
  }
}

void handle_screen_touch() {
  if (ts.touched()) {
    TS_Point p = ts.getPoint();

    // Scale from ~0->4000 to tft.width using the calibration #'s
    p.x = map(p.x, TS_MAXX, TS_MINX, 0, tft.width());
    p.y = map(p.y, TS_MAXY, TS_MINY, 0, tft.height());

    // Invert y axis to put origin at top left.
    p.y = tft.height() - p.y;

    Rect left_button = left_navigate_bounding_box();
    Rect right_button = right_navigate_bounding_box();

    if (shows_left_navigate_button() && p.x >= left_button.x &&
        p.x <= (left_button.x + left_button.w) && p.y >= left_button.y &&
        (p.y <= left_button.y + left_button.h)) {
      current_screen--;
      ticker_prices[current_screen].needs_draw = true;
      write_stock_screen();
    } else if (shows_right_navigate_button() && p.x >= right_button.x &&
               p.x <= (right_button.x + right_button.w) &&
               p.y >= right_button.y &&
               (p.y <= right_button.y + right_button.h)) {
      current_screen++;
      ticker_prices[current_screen].needs_draw = true;
      write_stock_screen();
    }
  }
}

void setup() {
  Serial.begin(115200);

  init_ts();
  init_tft();
  init_wifi();
  init_config_server();
  init_http_requests();

  while (!WiFi.isConnected())
    ;
  write_splash_screen();

  init_default_ticker_symbols();
}

void loop() {
  if ((WiFi.status() == WL_CONNECTED) &&
      (changed_configuration ||
       ((last_fetch + fetch_interval(previousError)) <= millis()))) {
    changed_configuration = false;
    update_ticker_prices();
    clamp_current_screen();
    write_stock_screen();
    last_fetch = millis();
  }

  handle_screen_touch();

  delay(100);
}