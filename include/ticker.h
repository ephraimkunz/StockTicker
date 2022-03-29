// Control the fetching of stock prices from Yahoo Finance.

#ifndef TICKER_H
#define TICKER_H

#include <constants.h>

// Represents a symbol, possible price (if there were no errors fetching), and whether the screen displaying
// this symbol needs to be updated.
struct TickerPrice {
  char symbol[MAX_SYMBOL_LENGTH];
  std::optional<float> price;
  bool needs_draw;
};

// Number of active ticker prices in ticker_prices
extern int ticker_prices_size;

// Array of ticker prices. The number of active items in this array is ticker_prices_size.
extern TickerPrice ticker_prices[MAX_NUM_TICKER_PRICES];

// Whether or not the last fetch of active ticker items resulted in an error.
extern bool ticker_has_previous_error;

// Timestamp in milliseconds of the last fetch of ticker items.
extern unsigned long ticker_last_fetch_ms;

// Inits the HTTP fetch infrastructure and sets up the default ticker symbols.
void ticker_init();

// Fetches the stock prices from Yahoo Finance.
void ticker_update_prices();

// How often to fetch the stock prices from Yahoo Finance, in milliseconds.
unsigned long ticker_fetch_interval_ms(bool has_previous_error);

#endif // TICKER_H