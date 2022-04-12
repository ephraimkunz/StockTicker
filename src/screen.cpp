#include <screen.h>
#include <scripture.h>
#include <ticker.h>

#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <ESP8266WiFi.h>
#include <Wire.h>
#include <XPT2046_Touchscreen.h>

// Which page index the user is currently on.
static int current_screen = 0;

static const int SPLASH_SCREEN = -2;
static const int SCRIPTURE_SCREEN = -1;

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

static Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);
static XPT2046_Touchscreen ts(TS_CS);

struct Rect {
  int x;
  int y;
  int w;
  int h;
};

enum ArrowButtonPosition { MIDDLE, BOTTOM };

static const int NAVIGATE_BUTTON_RADIUS = 20;
static const int NAVIGATE_BUTTON_PADDING = 5;

static bool shows_left_navigate_button() { return current_screen >= 0; }

static bool shows_right_navigate_button() {
  return current_screen < (ticker_prices_size - 1) ||
         current_screen == SCRIPTURE_SCREEN;
}

static Rect left_navigate_bounding_box(ArrowButtonPosition position) {
  switch (position) {
  case MIDDLE:
  default:
    return Rect{NAVIGATE_BUTTON_PADDING,
                (tft.height() / 2) - NAVIGATE_BUTTON_RADIUS,
                NAVIGATE_BUTTON_RADIUS * 2, NAVIGATE_BUTTON_RADIUS * 2};
  case BOTTOM:
    return Rect{NAVIGATE_BUTTON_PADDING,
                tft.height() - (NAVIGATE_BUTTON_RADIUS * 2),
                NAVIGATE_BUTTON_RADIUS * 2, NAVIGATE_BUTTON_RADIUS * 2};
  };
}

static Rect right_navigate_bounding_box(ArrowButtonPosition position) {
  switch (position) {
  case MIDDLE:
  default:
    return Rect{tft.width() - NAVIGATE_BUTTON_RADIUS - NAVIGATE_BUTTON_RADIUS -
                    NAVIGATE_BUTTON_PADDING,
                (tft.height() / 2) - NAVIGATE_BUTTON_RADIUS,
                NAVIGATE_BUTTON_RADIUS * 2, NAVIGATE_BUTTON_RADIUS * 2};
  case BOTTOM:
    return Rect{tft.width() - NAVIGATE_BUTTON_RADIUS - NAVIGATE_BUTTON_RADIUS -
                    NAVIGATE_BUTTON_PADDING,
                tft.height() - (NAVIGATE_BUTTON_RADIUS * 2),
                NAVIGATE_BUTTON_RADIUS * 2, NAVIGATE_BUTTON_RADIUS * 2};
  };
}

void screen_write_splash_screen() {
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

static void screen_write_left_arrow(ArrowButtonPosition position) {
  if (shows_left_navigate_button()) {
    Rect r = left_navigate_bounding_box(position);
    tft.fillCircle(r.x + r.w / 2, r.y + r.h / 2, NAVIGATE_BUTTON_RADIUS,
                   ILI9341_GREEN);
    tft.setCursor(r.x + (r.w / 2) - 10, r.y + (r.h / 2) - 10);
    tft.setTextColor(ILI9341_BLACK);
    tft.setTextSize(3);
    tft.print("<");
  }
}

static void screen_write_right_arrow(ArrowButtonPosition position) {
  if (shows_right_navigate_button()) {
    Rect r = right_navigate_bounding_box(position);
    tft.fillCircle(r.x + r.w / 2, r.y + r.h / 2, NAVIGATE_BUTTON_RADIUS,
                   ILI9341_GREEN);
    tft.setCursor(r.x + (r.w / 2) - 6, r.y + (r.h / 2) - 10);
    tft.setTextColor(ILI9341_BLACK);
    tft.setTextSize(3);
    tft.print(">");
  }
}

void screen_write_scripture_screen() {
  tft.fillScreen(ILI9341_BLACK);
  tft.setCursor(0, 0);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(3);
  tft.println("Daily Scripture");
  tft.println();

  tft.setTextSize(2);
  tft.println(scripture.verse_title);
  tft.println();
  tft.println(scripture.verse_text);

  screen_write_right_arrow(BOTTOM);
  screen_write_left_arrow(BOTTOM);
}

void screen_update_if_needed() {
  if (current_screen == SPLASH_SCREEN) {
    screen_write_splash_screen();
    return;
  }

  if (current_screen == SCRIPTURE_SCREEN) {
    screen_write_scripture_screen();
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

  screen_write_right_arrow(MIDDLE);
  screen_write_left_arrow(MIDDLE);

  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(1);
  tft.setCursor(20, tft.height() - 20);
  tft.print(WiFi.localIP());
}

static void init_tft() {
  tft.begin();
  tft.setRotation(1);
  screen_write_splash_screen();
}

static void init_ts() {
  ts.begin();
  ts.setRotation(1);
}

void screen_handle_touch() {
  if (ts.touched()) {
    TS_Point p = ts.getPoint();

    // Scale from ~0->4000 to tft.width using the calibration #'s
    p.x = map(p.x, TS_MAXX, TS_MINX, 0, tft.width());
    p.y = map(p.y, TS_MAXY, TS_MINY, 0, tft.height());

    // Invert y axis to put origin at top left.
    p.y = tft.height() - p.y;

    ArrowButtonPosition position =
        current_screen == SCRIPTURE_SCREEN ? BOTTOM : MIDDLE;

    Rect left_button = left_navigate_bounding_box(position);
    Rect right_button = right_navigate_bounding_box(position);

    if (shows_left_navigate_button() && p.x >= left_button.x &&
        p.x <= (left_button.x + left_button.w) && p.y >= left_button.y &&
        (p.y <= left_button.y + left_button.h)) {
      current_screen--;

      if (current_screen >= 0) {
        ticker_prices[current_screen].needs_draw = true;
      }
      screen_update_if_needed();
    } else if (shows_right_navigate_button() && p.x >= right_button.x &&
               p.x <= (right_button.x + right_button.w) &&
               p.y >= right_button.y &&
               (p.y <= right_button.y + right_button.h)) {
      current_screen++;
      ticker_prices[current_screen].needs_draw = true;
      screen_update_if_needed();
    }
  }
}

void screen_clamp_current_screen() {
  if (current_screen >= ticker_prices_size) {
    current_screen = 0;
  }

  if (ticker_prices_size == 0) {
    current_screen = SPLASH_SCREEN;
  }
}

void screen_init() {
  init_tft();
  init_ts();
}