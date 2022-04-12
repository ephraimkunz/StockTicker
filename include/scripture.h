// Control the fetching of a random scripture verse from the Rust backend.

#ifndef SCRIPTURE_H
#define SCRIPTURE_H

#include <constants.h>

#define MAX_VERSE_TITLE_LENGTH 50
#define MAX_VERSE_TEXT_LENGTH 500

// Represents a symbol, possible price (if there were no errors fetching), and
// whether the screen displaying this symbol needs to be updated.
struct Scripture {
  char verse_title[MAX_VERSE_TITLE_LENGTH];
  char verse_text[MAX_VERSE_TEXT_LENGTH];
};

// The scripture for today.
extern Scripture scripture;

// Inits the HTTP fetch infrastructure.
void scripture_init();

// Fetches the scripture
void scripture_update();

#endif // SCRIPTURE_H