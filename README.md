# M4 Express NeoPixel Blinky

A Zephyr RTOS application demonstrating NeoPixel (WS2812) control on the
Adafruit Feather M4 Express CAN board.

## Features

- Custom bit-banged WS2812 driver optimized for SAMD51 @ 120MHz
- Support for RGBW NeoPixels (4-channel)
- Separate power GPIO control (required for Feather M4 CAN)
- USB CDC console for debug output

## Hardware

- **Board:** Adafruit Feather M4 Express CAN
- **NeoPixel Data:** PB02
- **NeoPixel Power:** PB03

## Building

```bash
# Set up Zephyr environment first
source ~/zephyrproject/zephyr/zephyr-env.sh

# Build
west build -b adafruit_feather_m4_express

# Flash (via UF2 bootloader)
west flash
```

## Project Structure

```
m4-express-blinky/
├── CMakeLists.txt              # Build configuration
├── prj.conf                    # Kconfig settings
├── boards/
│   └── adafruit_feather_m4_express.overlay  # Device tree overlay
├── dts/
│   └── bindings/
│       └── worldsemi,ws2812-gpio-power.yaml # Custom binding
└── src/
    ├── main.c                  # Application code
    └── ws2812_gpio_sam0.c      # WS2812 driver for SAM0
```

## License

SPDX-License-Identifier: Apache-2.0

