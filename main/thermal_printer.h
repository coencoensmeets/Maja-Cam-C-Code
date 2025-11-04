#ifndef THERMAL_PRINTER_H
#define THERMAL_PRINTER_H

#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include <stdbool.h>

/**
 * @file thermal_printer.h
 * @brief Thermal printer driver for EM205 and compatible printers
 */

// Forward declaration
typedef struct ThermalPrinter ThermalPrinter_t;

// Printer configuration
typedef struct {
    uart_port_t uart_port;      // UART port (UART_NUM_1 or UART_NUM_2)
    gpio_num_t tx_pin;           // TX pin (ESP32 -> Printer RX)
    gpio_num_t rx_pin;           // RX pin (ESP32 <- Printer TX)
    gpio_num_t rts_pin;          // RTS pin (optional, set to -1 if not used)
    int baud_rate;               // Baud rate (default 9600)
    int max_print_width;         // Maximum characters per line (default 32)
} printer_config_t;

// Printer object structure
struct ThermalPrinter {
    // Configuration
    printer_config_t config;
    
    // State
    bool initialized;
    
    // Methods
    esp_err_t (*init)(ThermalPrinter_t *self);
    esp_err_t (*print_text)(ThermalPrinter_t *self, const char *text);
    esp_err_t (*print_line)(ThermalPrinter_t *self, const char *text);
    esp_err_t (*feed_lines)(ThermalPrinter_t *self, int lines);
    esp_err_t (*set_align)(ThermalPrinter_t *self, int align); // 0=left, 1=center, 2=right
    esp_err_t (*set_bold)(ThermalPrinter_t *self, bool enabled);
    esp_err_t (*set_size)(ThermalPrinter_t *self, int width, int height); // 1-8
    esp_err_t (*cut_paper)(ThermalPrinter_t *self);
    void (*destroy)(ThermalPrinter_t *self);
};

/**
 * @brief Create thermal printer object
 * 
 * @param config Printer configuration
 * @return ThermalPrinter_t* Pointer to printer object or NULL on failure
 */
ThermalPrinter_t *thermal_printer_create(printer_config_t config);

/**
 * @brief Destroy thermal printer object
 * 
 * @param printer Pointer to printer object
 */
void thermal_printer_destroy(ThermalPrinter_t *printer);

/**
 * @brief Print a poem with nice formatting
 * 
 * @param printer Pointer to printer object
 * @param title Title of the poem (e.g., "Sunset at the Beach")
 * @param poet_style Poet style (e.g., "Shakespeare", "Haiku")
 * @param poem_text The poem text
 * @return esp_err_t ESP_OK on success
 */
esp_err_t thermal_printer_print_poem(ThermalPrinter_t *printer, 
                                      const char *title,
                                      const char *poet_style,
                                      const char *poem_text);

#endif // THERMAL_PRINTER_H
