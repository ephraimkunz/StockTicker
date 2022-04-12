// Handles the config server that accepts the ticker symbols to fetch from the
// user.

#ifndef CONFIG_SERVER_H
#define CONFIG_SERVER_H

#include <constants.h>

// Set by the config server when the user changes the config. Should be cleared
// by other code once the new config is read.
extern bool config_server_changed_configuration;

// The number of active symbols in config_ticker_symbols_to_fetch.
extern int config_num_ticker_symbols_to_fetch;

// Symbols the user has specified should be displayed. The number of active
// items in this array is config_num_ticker_symbols_to_fetch.
extern char config_ticker_symbols_to_fetch[MAX_NUM_TICKER_PRICES]
                                          [MAX_SYMBOL_LENGTH];

// Initialize the config server, to be called once in setup().
void config_server_init();

#endif // CONFIG_SERVER_H