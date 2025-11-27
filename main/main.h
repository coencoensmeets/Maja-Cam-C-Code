#ifndef MAIN_H
#define MAIN_H

#include "ota_manager.h"

// Global OTA manager instance (defined in main.c)
extern OTAManager_t *g_ota_manager;

// Function to stop the poem loading animation
// Call this when a poem is received/printed to stop the magenta loading animation
void stop_poem_loading_animation(void);

#endif // MAIN_H
