// Control over the combination touchscreen and LCD.

#ifndef SCREEN_H
#define SCREEN_H

// Initialize the LCD + touch screen, to be called once in setup().
void screen_init();

// Write the splash screen to the screen.
void screen_write_splash_screen();

// Update the screen based on the current screen index, if it needs to be
// updated.
void screen_update_if_needed();

// Handle any touch events by updating the state and screen.
void screen_handle_touch();

// Update the current screen global to be within the bounds of fetched stock
// information.
void screen_clamp_current_screen();

#endif // SCREEN_H