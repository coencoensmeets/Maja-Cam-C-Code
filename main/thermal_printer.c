#include "thermal_printer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>

static const char *TAG = "THERMAL_PRINTER";

// ESC/POS Commands
#define ESC 0x1B
#define GS  0x1D

// Initialize printer
static esp_err_t thermal_printer_init_impl(ThermalPrinter_t *self)
{
    ESP_LOGI(TAG, "Initializing thermal printer on UART%d", self->config.uart_port);
    ESP_LOGI(TAG, "TX: GPIO%d, RX: GPIO%d, RTS: GPIO%d, Baud: %d", 
             self->config.tx_pin, self->config.rx_pin, self->config.rts_pin, self->config.baud_rate);
    
    // Configure UART
    uart_config_t uart_config = {
        .baud_rate = self->config.baud_rate,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    
    esp_err_t err;
    
    // Delete UART driver if it already exists (cleanup from previous init)
    uart_driver_delete(self->config.uart_port);
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Install UART driver with larger buffers
    err = uart_driver_install(self->config.uart_port, 2048, 2048, 0, NULL, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install UART driver: %s", esp_err_to_name(err));
        return err;
    }
    
    // Configure UART parameters
    err = uart_param_config(self->config.uart_port, &uart_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure UART: %s", esp_err_to_name(err));
        uart_driver_delete(self->config.uart_port);
        return err;
    }
    
    // Set UART pins (including RTS if specified)
    gpio_num_t rts = (self->config.rts_pin >= 0) ? self->config.rts_pin : UART_PIN_NO_CHANGE;
    err = uart_set_pin(self->config.uart_port, 
                      self->config.tx_pin, 
                      self->config.rx_pin,
                      rts, 
                      UART_PIN_NO_CHANGE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set UART pins: %s", esp_err_to_name(err));
        uart_driver_delete(self->config.uart_port);
        return err;
    }
    
    // Flush any pending data
    uart_flush(self->config.uart_port);
    uart_flush_input(self->config.uart_port);
    
    // Wait for printer to be ready
    vTaskDelay(pdMS_TO_TICKS(500)); // Increased wait time
    
    // Send printer initialization command
    uint8_t init_cmd[] = {ESC, '@'}; // Initialize printer
    ESP_LOGI(TAG, "Sending ESC @ (init command): 0x%02X 0x%02X", init_cmd[0], init_cmd[1]);
    uart_write_bytes(self->config.uart_port, (const char *)init_cmd, sizeof(init_cmd));
    uart_wait_tx_done(self->config.uart_port, pdMS_TO_TICKS(200));
    
    vTaskDelay(pdMS_TO_TICKS(500)); // Wait longer for reset to complete
    
    // Try setting international character set (might help)
    uint8_t intl_cmd[] = {ESC, 'R', 0x00}; // USA
    ESP_LOGI(TAG, "Setting international charset: ESC R 0");
    uart_write_bytes(self->config.uart_port, (const char *)intl_cmd, sizeof(intl_cmd));
    uart_wait_tx_done(self->config.uart_port, pdMS_TO_TICKS(200));
    
    vTaskDelay(pdMS_TO_TICKS(200));
    
    // Disable Chinese character mode explicitly
    uint8_t chinese_off[] = {0x1C, 0x26}; // Cancel Chinese character mode (FS &)
    ESP_LOGI(TAG, "Disabling Chinese mode: 0x1C 0x26");
    uart_write_bytes(self->config.uart_port, (const char *)chinese_off, sizeof(chinese_off));
    uart_wait_tx_done(self->config.uart_port, pdMS_TO_TICKS(200));
    
    vTaskDelay(pdMS_TO_TICKS(200));
    
    self->initialized = true;
    ESP_LOGI(TAG, "Thermal printer initialized successfully (ready for printing)");
    
    return ESP_OK;
}

// Print text without line feed
static esp_err_t thermal_printer_print_text_impl(ThermalPrinter_t *self, const char *text)
{
    if (!self->initialized) {
        ESP_LOGE(TAG, "Printer not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!text) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Log what we're sending to the printer
    ESP_LOGI(TAG, "PRINT: \"%s\"", text);
    
    int bytes_written = uart_write_bytes(self->config.uart_port, text, strlen(text));
    ESP_LOGI(TAG, "Wrote %d bytes to UART", bytes_written);
    
    if (bytes_written < 0) {
        ESP_LOGE(TAG, "UART write error!");
        return ESP_FAIL;
    }
    
    // Wait for UART to finish transmitting
    esp_err_t err = uart_wait_tx_done(self->config.uart_port, pdMS_TO_TICKS(1000));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "UART TX timeout: %s", esp_err_to_name(err));
    }
    
    return ESP_OK;
}

// Print text with line feed
static esp_err_t thermal_printer_print_line_impl(ThermalPrinter_t *self, const char *text)
{
    if (!self->initialized) {
        ESP_LOGE(TAG, "Printer not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (text) {
        // Log what we're sending to the printer
        ESP_LOGI(TAG, "PRINT LINE: \"%s\"", text);
        int bytes_written = uart_write_bytes(self->config.uart_port, text, strlen(text));
        ESP_LOGI(TAG, "Wrote %d bytes to UART", bytes_written);
    }
    // Use \r\n for better printer compatibility
    uart_write_bytes(self->config.uart_port, "\r\n", 2);
    
    // Wait for UART to finish transmitting
    esp_err_t err = uart_wait_tx_done(self->config.uart_port, pdMS_TO_TICKS(1000));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "UART TX timeout: %s", esp_err_to_name(err));
    }
    
    // Add much longer delay to give printer time to process and print the line
    vTaskDelay(pdMS_TO_TICKS(500)); // Increased from 400ms to 500ms
    
    return ESP_OK;
}

// Feed paper lines
static esp_err_t thermal_printer_feed_lines_impl(ThermalPrinter_t *self, int lines)
{
    if (!self->initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    for (int i = 0; i < lines; i++) {
        uart_write_bytes(self->config.uart_port, "\r\n", 2);
        vTaskDelay(pdMS_TO_TICKS(50)); // Give printer time to feed
    }
    
    return ESP_OK;
}

// Set text alignment
static esp_err_t thermal_printer_set_align_impl(ThermalPrinter_t *self, int align)
{
    if (!self->initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    uint8_t cmd[] = {ESC, 'a', (uint8_t)(align & 0x03)};
    uart_write_bytes(self->config.uart_port, (const char *)cmd, sizeof(cmd));
    
    return ESP_OK;
}

// Set bold text
static esp_err_t thermal_printer_set_bold_impl(ThermalPrinter_t *self, bool enabled)
{
    if (!self->initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    uint8_t cmd[] = {ESC, 'E', enabled ? 1 : 0};
    uart_write_bytes(self->config.uart_port, (const char *)cmd, sizeof(cmd));
    
    return ESP_OK;
}

// Set text size
static esp_err_t thermal_printer_set_size_impl(ThermalPrinter_t *self, int width, int height)
{
    if (!self->initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // Limit to valid range (1-8)
    if (width < 1) width = 1;
    if (width > 8) width = 8;
    if (height < 1) height = 1;
    if (height > 8) height = 8;
    
    uint8_t size = ((width - 1) << 4) | (height - 1);
    uint8_t cmd[] = {GS, '!', size};
    uart_write_bytes(self->config.uart_port, (const char *)cmd, sizeof(cmd));
    
    return ESP_OK;
}

// Cut paper
static esp_err_t thermal_printer_cut_paper_impl(ThermalPrinter_t *self)
{
    if (!self->initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // Feed some paper before cutting
    thermal_printer_feed_lines_impl(self, 4);
    
    // Full cut command (if supported)
    uint8_t cmd[] = {GS, 'V', 0};
    uart_write_bytes(self->config.uart_port, (const char *)cmd, sizeof(cmd));
    
    vTaskDelay(pdMS_TO_TICKS(500)); // Wait for cut
    
    return ESP_OK;
}

// Destroy printer object
static void thermal_printer_destroy_impl(ThermalPrinter_t *self)
{
    if (self) {
        if (self->initialized) {
            uart_driver_delete(self->config.uart_port);
        }
        free(self);
    }
}

// Constructor
ThermalPrinter_t *thermal_printer_create(printer_config_t config)
{
    ThermalPrinter_t *printer = malloc(sizeof(ThermalPrinter_t));
    if (!printer) {
        ESP_LOGE(TAG, "Failed to allocate memory for printer");
        return NULL;
    }
    
    // Set configuration
    printer->config = config;
    printer->initialized = false;
    
    // Bind methods
    printer->init = thermal_printer_init_impl;
    printer->print_text = thermal_printer_print_text_impl;
    printer->print_line = thermal_printer_print_line_impl;
    printer->feed_lines = thermal_printer_feed_lines_impl;
    printer->set_align = thermal_printer_set_align_impl;
    printer->set_bold = thermal_printer_set_bold_impl;
    printer->set_size = thermal_printer_set_size_impl;
    printer->cut_paper = thermal_printer_cut_paper_impl;
    printer->destroy = thermal_printer_destroy_impl;
    
    return printer;
}

// Destructor
void thermal_printer_destroy(ThermalPrinter_t *printer)
{
    if (printer && printer->destroy) {
        printer->destroy(printer);
    }
}

// Helper function to wrap text to max width (word wrapping)
static void print_wrapped_line(ThermalPrinter_t *printer, const char *text, int max_width)
{
    if (!text || strlen(text) == 0) {
        printer->print_line(printer, "");
        return;
    }
    
    ESP_LOGI(TAG, "Word-wrapping line (max_width=%d): '%s'", max_width, text);
    
    // Count leading spaces for indentation
    int leading_spaces = 0;
    while (text[leading_spaces] == ' ' && leading_spaces < strlen(text)) {
        leading_spaces++;
    }
    
    // For thermal printer, reduce indentation by half and limit to max 8 spaces
    leading_spaces = leading_spaces / 2;
    if (leading_spaces > 8) {
        leading_spaces = 8;
    }
    
    // Get the actual text content (skip leading spaces)
    const char *content = text;
    while (*content == ' ' && *content != '\0') {
        content++;
    }
    
    // If the line with limited indentation fits, print it directly
    if (leading_spaces + strlen(content) <= max_width) {
        char indented_line[max_width + 1];
        int i;
        for (i = 0; i < leading_spaces; i++) {
            indented_line[i] = ' ';
        }
        strncpy(indented_line + leading_spaces, content, max_width - leading_spaces);
        indented_line[max_width] = '\0';
        printer->print_line(printer, indented_line);
        return;
    }
    
    // Need to wrap - split at word boundaries, preserving initial indentation
    char *text_copy = strdup(content);
    if (!text_copy) {
        ESP_LOGE(TAG, "Failed to strdup in print_wrapped_line, printing as-is");
        printer->print_line(printer, text); // Fallback
        return;
    }
    
    char buffer[max_width + 1];
    int pos = 0;
    bool is_first_line = true;
    char *saveptr = NULL;  // Use reentrant strtok_r instead of strtok
    char *word = strtok_r(text_copy, " ", &saveptr);
    
    while (word) {
        int word_len = strlen(word);
        
        // If adding this word exceeds max width, print current buffer and start new line
        if (pos > 0 && pos + word_len + 1 > max_width) {
            buffer[pos] = '\0';
            printer->print_line(printer, buffer);
            pos = 0;
            is_first_line = false;
        }
        
        // Add indentation for first line or continuation indent
        if (pos == 0) {
            if (is_first_line) {
                // Add original indentation to first line
                for (int i = 0; i < leading_spaces && i < max_width; i++) {
                    buffer[pos++] = ' ';
                }
            } else {
                // Add continuation indent (2 spaces beyond original indent)
                int continuation_indent = leading_spaces + 2;
                if (continuation_indent > max_width - 5) {
                    continuation_indent = (max_width > 5) ? 2 : 0;
                }
                for (int i = 0; i < continuation_indent && i < max_width; i++) {
                    buffer[pos++] = ' ';
                }
            }
        }
        
        // Add word to buffer
        if (pos > 0 && buffer[pos-1] != ' ' && pos < max_width) {
            buffer[pos++] = ' '; // Add space before word
        }
        
        // Copy word to buffer (with bounds checking)
        int chars_to_copy = word_len;
        if (pos + chars_to_copy >= max_width) {
            chars_to_copy = max_width - pos - 1;
        }
        if (chars_to_copy > 0) {
            strncpy(buffer + pos, word, chars_to_copy);
            pos += chars_to_copy;
        }
        
        word = strtok_r(NULL, " ", &saveptr);
    }
    
    // Print remaining text in buffer
    if (pos > 0) {
        buffer[pos] = '\0';
        printer->print_line(printer, buffer);
    }
    
    free(text_copy);
}

// Get a random decorative line for poem borders
static void print_random_decorative_border(ThermalPrinter_t *printer) {
    // 10 different decorative line options (some multi-line)
    int pattern = rand() % 10;
    
    switch(pattern) {
        case 0: // Original stars
            printer->print_line(printer, "~*~*~*~*~*~*~*~*~*~*~*~*~*~*");
            break;
        case 1: // Dashes and equals
            printer->print_line(printer, "-=-=-=-=-=-=-=-=-=-=-=-=-=-=");
            break;
        case 2: // Simple dots
            printer->print_line(printer, "................................");
            break;
        case 3: // Hash marks
            printer->print_line(printer, "################################");
            break;
        case 4: // Double lines
            printer->print_line(printer, "================================");
            break;
        case 5: // Asterisks
            printer->print_line(printer, "********************************");
            break;
        case 6: // Plus signs
            printer->print_line(printer, "++++++++++++++++++++++++++++++++");
            break;
        case 7: // Chevrons pattern (multi-line)
            printer->print_line(printer, "   `     `     `     `     `");
            printer->print_line(printer, "  ' . ' ' . ' ' . ' ' . ' ' .");
            printer->print_line(printer, "   `     `     `     `     `");
            break;
        case 8: // Vertical bars
            printer->print_line(printer, "||||||||||||||||||||||||||||||||");
            break;
        case 9: // Wave pattern (multi-line)
            printer->print_line(printer, " .  '  .  '  .  '  .  '  .  '");
            printer->print_line(printer, "'  .  '  .  '  .  '  .  '  .");
            break;
    }
}

// Print a nicely formatted poem
esp_err_t thermal_printer_print_poem(ThermalPrinter_t *printer, 
                                      const char *title,
                                      const char *poet_style,
                                      const char *poem_text)
{
    if (!printer || !printer->initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Printing poem: %s", title ? title : "Untitled");
    
    // Seed random number generator with current time
    srand(time(NULL));
    
    // Store random pattern choice to use same pattern throughout
    int border_pattern = rand() % 10;
    
    // Print decorative top border
    printer->set_align(printer, 1); // Center
    printer->set_size(printer, 1, 1); // Normal size
    printer->feed_lines(printer, 1);
    
    // Use the stored pattern
    srand(border_pattern);
    print_random_decorative_border(printer);
    
    printer->feed_lines(printer, 1);
    
    // Print title if provided
    if (title && strlen(title) > 0) {
        printer->set_bold(printer, true);
        printer->set_size(printer, 1, 1);
        printer->print_line(printer, title);
        printer->set_bold(printer, false);
        printer->feed_lines(printer, 1);
    }
    
    // Use the same pattern again
    srand(border_pattern);
    print_random_decorative_border(printer);
    
    printer->feed_lines(printer, 2);
    
    // Print poem text (left aligned)
    printer->set_align(printer, 0); // Left align for poem
    printer->set_size(printer, 1, 1);
    
    if (poem_text) {
        // Print each line of the poem with word wrapping
        // We need to avoid nested strtok, so we'll manually parse lines
        char *poem_copy = strdup(poem_text);
        if (poem_copy) {
            char *current = poem_copy;
            char *line_start = current;
            int line_count = 0;
            
            ESP_LOGI(TAG, "Starting to parse poem text (%d chars total)", strlen(poem_text));
            
            while (*current != '\0') {
                if (*current == '\n') {
                    // Found end of line
                    *current = '\0';
                    line_count++;
                    ESP_LOGI(TAG, "Printing poem line %d: '%s'", line_count, line_start);
                    // Use word wrapping for each line (max 32 chars per line)
                    print_wrapped_line(printer, line_start, printer->config.max_print_width);
                    current++;
                    line_start = current;
                } else {
                    current++;
                }
            }
            
            // Print the last line if there's any remaining text
            if (line_start < current && *line_start != '\0') {
                line_count++;
                ESP_LOGI(TAG, "Printing final poem line %d: '%s'", line_count, line_start);
                print_wrapped_line(printer, line_start, printer->config.max_print_width);
            }
            
            ESP_LOGI(TAG, "Finished printing %d lines of poem", line_count);
            free(poem_copy);
        } else {
            ESP_LOGE(TAG, "Failed to allocate memory for poem copy!");
        }
    }
    
    // Print footer
    printer->feed_lines(printer, 2);
    printer->set_align(printer, 1); // Center
    
    // Use the same pattern for footer
    srand(border_pattern);
    print_random_decorative_border(printer);
    
    printer->feed_lines(printer, 4);
    
    ESP_LOGI(TAG, "Poem printed successfully");
    return ESP_OK;
}
