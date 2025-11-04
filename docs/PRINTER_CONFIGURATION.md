# Thermal Printer Configuration

## Overview
The ESP32 Poem Camera supports thermal printing via UART. Printer settings are now configurable through the `settings.json` file stored in SPIFFS.

## Configuration File Location
- **File**: `data/settings.json`
- **Storage**: Uploaded to SPIFFS partition on ESP32
- **Format**: JSON

## Thermal Printer Settings

Add the following section to your `settings.json`:

```json
{
  "thermal_printer": {
    "enabled": true,
    "uart_port": 1,
    "tx_pin": 41,
    "rx_pin": 42,
    "rts_pin": 2,
    "baud_rate": 9600,
    "max_print_width": 32
  }
}
```

### Settings Explained

| Setting | Type | Default | Description |
|---------|------|---------|-------------|
| `enabled` | boolean | `true` | Enable/disable thermal printer support |
| `uart_port` | integer | `1` | UART port number (1 or 2) |
| `tx_pin` | integer | `41` | GPIO pin for TX (ESP32 → Printer RX) |
| `rx_pin` | integer | `42` | GPIO pin for RX (ESP32 ← Printer TX) |
| `rts_pin` | integer | `2` | GPIO pin for RTS flow control (-1 to disable) |
| `baud_rate` | integer | `9600` | Communication baud rate (EM205: 9600) |
| `max_print_width` | integer | `32` | Maximum characters per line |

## Supported Printers

### EM205 Thermal Printer
- **Baud Rate**: 9600 (verified working)
- **Protocol**: ESC/POS commands
- **Power**: External 5V supply required
- **Wiring**:
  - ESP32 TX (GPIO 41) → Printer RX
  - ESP32 RX (GPIO 42) → Printer TX
  - ESP32 RTS (GPIO 2) → Printer RTS
  - GND → GND
  - External 5V power supply

## Changing Pin Configuration

To use different GPIO pins:

1. Edit `data/settings.json`
2. Update `tx_pin`, `rx_pin`, `rts_pin` values
3. Upload the updated file to SPIFFS
4. Reboot the ESP32

**Note**: Avoid using pins that conflict with:
- Console UART (GPIO 43/44 on ESP32-S3)
- Camera interface pins
- LED ring control pins
- Rotary encoder pins

## Disabling the Printer

To disable thermal printing without removing hardware:

```json
{
  "thermal_printer": {
    "enabled": false
  }
}
```

## Testing

On boot, if the printer is enabled and successfully initialized, you'll see:

```
I (xxxx) MAIN: --- Creating Thermal Printer ---
I (xxxx) MAIN: Printer config from settings: UART1, TX=41, RX=42, RTS=2, Baud=9600
I (xxxx) THERMAL_PRINTER: Sending ESC @ (init command): 0x1B 0x40
I (xxxx) THERMAL_PRINTER: Thermal printer initialized successfully
I (xxxx) MAIN: Thermal Printer initialized successfully at 9600 baud
I (xxxx) MAIN: Sending test print...
```

A test receipt will print:
```
=== PRINTER TEST ===
ESP32 Poem Camera
Printer Ready!
```

## Troubleshooting

### No output from printer
1. Check baud rate matches your printer (EM205: 9600)
2. Verify wiring: TX↔RX, RX↔TX (crossover)
3. Ensure external 5V power is connected
4. Check ESP-IDF Monitor for initialization errors

### Gibberish output
- Wrong baud rate (try 9600, 19200, 38400)
- Check signal inversion (EM205 uses standard logic)

### Printer not initializing
- Verify GPIO pins are not used by other peripherals
- Check `settings.json` uploaded to SPIFFS correctly
- Review ESP logs for error messages

## Advanced: Multiple Printers

To support different printer models, create separate configuration profiles and switch `baud_rate` as needed:

- **EM205**: 9600 baud
- **Generic ESC/POS**: 9600 or 19200 baud  
- **Some receipt printers**: 38400 or 115200 baud

## Code References

- **Driver**: `main/thermal_printer.c`
- **Settings**: `main/settings_manager.c`
- **Initialization**: `main/main.c` (line ~318)
- **Print Handler**: `main/remote_control.c`
