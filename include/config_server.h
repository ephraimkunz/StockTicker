// Handles the config server that accepts the ticker symbols to fetch from the
// user.

#ifndef CONFIG_SERVER_H
#define CONFIG_SERVER_H

#include <constants.h>

// Set by the config server when the user changes the config. Should be cleared
// by other code once the new config is read.
extern bool config_server_changed_configuration;

int config_num_ticker_symbols();

// Returns the symbol at index the user has specified should be displayed. 
// The number of active items in this array can be determined by calling
// config_num_ticker_symbols().
char *config_ticker_symbol_at_index(int index);

// Initialize the config server, to be called once in setup().
void config_server_init();

// Init the default ticker symbols to be fetched immediately. Should be called
// after config_server_init, one time in setup.
void config_init_default_ticker_symbols();

#endif // CONFIG_SERVER_H