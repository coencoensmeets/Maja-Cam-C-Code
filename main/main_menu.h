#ifndef MAIN_MENU_H
#define MAIN_MENU_H

#include <stdbool.h>
#include "led_ring.h"
#include "settings_manager.h"

/**
 * @file main_menu.h
 * @brief Interface for main menu LED ring control
 * 
 * Provides functions to update and refresh the LED ring menu display
 * from other modules (e.g., web_server, remote_control).
 */

/**
 * @brief Initialize the main menu system
 * 
 * @param led_ring Pointer to LED ring object
 * @param settings Pointer to settings manager object
 */
void main_menu_init(LEDRing_t *led_ring, SettingsManager_t *settings);

/**
 * @brief Smooth fade in animation for menu
 * 
 * @param selected_option Menu option index (0-4) to highlight during fade-in
 */
void main_menu_fade_in(int selected_option);

/**
 * @brief Update LED ring to display menu with specified option selected
 * 
 * @param selected_option Menu option index (0-4) to highlight
 */
void update_led_ring_menu(int selected_option);

/**
 * @brief Get current menu option selection
 * 
 * @return Current selected menu option index (0-4)
 */
int get_current_menu_option(void);

/**
 * @brief Set current menu option
 * 
 * @param option Menu option index (0-4) to set as current
 */
void set_current_menu_option(int option);

/**
 * @brief Refresh LED ring menu display with current selection
 * 
 * Useful when brightness or other LED settings change and the
 * menu needs to be redrawn.
 */
void refresh_led_ring_menu(void);

/**
 * @brief Check if menu is currently visible
 * 
 * @return true if menu has been shown, false if LEDs are still off
 */
bool is_menu_visible(void);

/**
 * @brief Check if sub-menu is currently active
 * 
 * @return true if sub-menu is active, false if main menu
 */
bool is_sub_menu_active(void);

/**
 * @brief Enter sub-menu mode (half red OFF, half green ON)
 * 
 * @param initial_selection true for ON (green), false for OFF (red)
 */
void main_menu_enter_sub_menu(bool initial_selection);

/**
 * @brief Update sub-menu display based on selection
 * 
 * @param selection true for ON (green side bright), false for OFF (red side bright)
 */
void main_menu_update_sub_menu(bool selection);

/**
 * @brief Exit sub-menu and return to main mode (fade out)
 */
void main_menu_exit_sub_menu(void);

/**
 * @brief Get menu option name
 * 
 * @param option Menu option index (0-4)
 * @return Name of the menu option
 */
const char* get_menu_option_name(int option);

/**
 * @brief Get total number of menu options
 * 
 * @return Number of menu options
 */
int get_menu_options_count(void);

/**
 * @brief Reset the inactivity timer (call when encoder is rotated)
 * 
 * Prevents the menu from fading out by resetting the 3-second timer.
 */
void main_menu_reset_timer(void);

/**
 * @brief Stop the fade-out timer
 * 
 * Used when taking control of LEDs (e.g., self-timer countdown)
 */
void main_menu_stop_timer(void);

/**
 * @brief Fade out the menu after timeout
 * 
 * Called internally when the inactivity timer expires.
 */
void main_menu_fade_out(void);

#endif // MAIN_MENU_H
