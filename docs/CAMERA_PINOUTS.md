# Common ESP32-S3 Camera Pin Configurations

## Current Configuration (in camera.c)

```
XCLK:  GPIO 15
SDA:   GPIO 4
SCL:   GPIO 5
D0-D7: GPIO 11,9,8,10,12,18,17,16
VSYNC: GPIO 6
HREF:  GPIO 7
PCLK:  GPIO 13
```

## ESP32-S3 Freenove Camera Board (Common)

```
XCLK:  GPIO 15
SDA:   GPIO 4
SCL:   GPIO 5
D0:    GPIO 11
D1:    GPIO 9
D2:    GPIO 8
D3:    GPIO 10
D4:    GPIO 12
D5:    GPIO 18
D6:    GPIO 17
D7:    GPIO 16
VSYNC: GPIO 6
HREF:  GPIO 7
PCLK:  GPIO 13
PWDN:  -1 (not used)
RESET: -1 (not used)
```

## ESP32-S3-EYE (Alternative)

```
XCLK:  GPIO 15
SDA:   GPIO 4
SCL:   GPIO 5
D0:    GPIO 11
D1:    GPIO 9
D2:    GPIO 8
D3:    GPIO 10
D4:    GPIO 12
D5:    GPIO 18
D6:    GPIO 17
D7:    GPIO 16
VSYNC: GPIO 6
HREF:  GPIO 7
PCLK:  GPIO 13
PWDN:  GPIO 48
RESET: -1
```

## Troubleshooting

### I2C Timeout Errors

- **Check pull-up resistors**: I2C requires 4.7kΩ pull-ups on SDA and SCL
- **Verify power**: Camera needs stable 3.3V with sufficient current (>100mA)
- **Check connections**: Loose ribbon cables are common
- **Wrong address**: Some cameras use 0x30, others 0x3C

### Camera Not Detected

1. Measure voltage on camera power pins (should be 3.3V)
2. Check if XCLK is generating 20MHz signal (use oscilloscope/logic analyzer)
3. Verify I2C lines are high when idle (should be 3.3V with pull-ups)
4. Try different I2C pins if board allows
5. Test with known-good camera module

### Known Issues

- ESP32-S3 internal pull-ups (~45kΩ) may be too weak for some cameras
- Long ribbon cables need external pull-ups
- Some camera modules have built-in pull-ups, others don't
