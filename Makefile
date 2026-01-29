# Makefile wrapper for Zephyr/West build system
# Usage: make, make clean, make flash, make menuconfig

BOARD ?= feather_m4_can_express
APP_DIR := app
BUILD_DIR := build
ZEPHYR_SDK_INSTALL_DIR ?= /tlctank/git/zephyr-sdk/zephyr-sdk-0.17.4

# Export SDK path for west/cmake
export ZEPHYR_SDK_INSTALL_DIR

.PHONY: all build clean pristine flash menuconfig update setup help

# Default target
all: build

# Build the application
build:
	west build -b $(BOARD) $(APP_DIR)

# Clean build (pristine)
pristine:
	west build -p always -b $(BOARD) $(APP_DIR)

# Remove build directory
clean:
	rm -rf $(BUILD_DIR)

# Flash via UF2 (copies to mounted bootloader volume)
flash: build
	@if [ -d "/run/media/$(USER)/FEATHERBOOT" ]; then \
		cp $(BUILD_DIR)/zephyr/zephyr.uf2 /run/media/$(USER)/FEATHERBOOT/; \
		echo "Flashed to /run/media/$(USER)/FEATHERBOOT"; \
	elif [ -d "/media/$(USER)/FEATHERBOOT" ]; then \
		cp $(BUILD_DIR)/zephyr/zephyr.uf2 /media/$(USER)/FEATHERBOOT/; \
		echo "Flashed to /media/$(USER)/FEATHERBOOT"; \
	else \
		echo "ERROR: FEATHERBOOT volume not found. Double-tap reset to enter bootloader."; \
		exit 1; \
	fi

# Interactive Kconfig menu
menuconfig:
	west build -t menuconfig

# Update Zephyr and modules (run once after clone)
update:
	west update

# First-time setup
setup: update
	pip install -r zephyr/scripts/requirements.txt

help:
	@echo "Targets:"
	@echo "  make          - Build the application"
	@echo "  make pristine - Clean build from scratch"
	@echo "  make clean    - Remove build directory"
	@echo "  make flash    - Build and flash via UF2 bootloader"
	@echo "  make menuconfig - Open Kconfig menu"
	@echo "  make update   - Update Zephyr and modules"
	@echo "  make setup    - First-time setup (update + pip install)"
	@echo ""
	@echo "Variables:"
	@echo "  BOARD=<board> - Override target board (default: $(BOARD))"
