# M4 Express NeoPixel Blinky

A Zephyr RTOS application demonstrating NeoPixel (WS2812) control on the
Adafruit Feather M4 Express CAN board.

## Features

- Custom bit-banged WS2812 driver optimized for SAME51 @ 120MHz
- Support for RGBW NeoPixels (4-channel)
- Separate power GPIO control (required for Feather M4 CAN)
- USB CDC console for debug output

## Hardware

- **Board:** Adafruit Feather M4 Express CAN
- **NeoPixel Data:** PB02
- **NeoPixel Power:** PB03

Note that the official documentation is wrong. https://learn.adafruit.com/adafruit-feather-m4-can-express/pinouts

Express CAN has Neopixel Data on PB02, NOT on PB22 as indicated. PB03 is a power GPIO.


## Building

This is a self-contained Zephyr application with its own West manifest.

### First-time setup

```bash
# Clone the repository
git clone <repo-url> m4-can-express-blinky
cd m4-can-express-blinky

# Initialize and update West workspace (downloads Zephyr and dependencies)
west update

# Install Python dependencies
pip install -r zephyr/scripts/requirements.txt
```

### Build

```bash
# Set SDK path (add to ~/.bashrc for persistence)
export ZEPHYR_SDK_INSTALL_DIR=/path/to/zephyr-sdk-0.17.4

# Build (use -p always for clean build)
west build -p always -b feather_m4_can_express app

# Incremental rebuild after code changes
west build
```

### Flash

The Feather M4 uses a UF2 bootloader:

1. Double-tap reset button (LED pulses, `FEATHERBOOT` USB drive appears)
2. Copy the UF2 file:
   ```bash
   cp build/zephyr/zephyr.uf2 /run/media/$USER/FEATHERBOOT/
   ```

Or use west (if UF2 volume is auto-detected):
```bash
west flash
```

## Project Structure

This is a freestanding Zephyr application using the T3 "forest" topology:

```
m4-can-express-blinky/
├── .west/                      # West workspace config
├── app/                        # Application code (this repo)
│   ├── west.yml                # West manifest
│   ├── CMakeLists.txt          # Build configuration
│   ├── prj.conf                # Kconfig settings
│   ├── boards/
│   │   └── adafruit_feather_m4_express.overlay
│   ├── dts/
│   │   └── bindings/
│   │       └── worldsemi,ws2812-gpio-power.yaml
│   └── src/
│       ├── main.c              # Application code
│       └── ws2812_gpio_sam0.c  # WS2812 driver for SAM0
├── zephyr/                     # Zephyr RTOS (downloaded by west)
├── modules/                    # Zephyr modules (downloaded by west)
└── build/                      # Build output
```

## License

SPDX-License-Identifier: Apache-2.0

