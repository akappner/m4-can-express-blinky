# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

# Add custom DTS bindings directory
list(APPEND DTS_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(m4_express_blinky)

target_sources(app PRIVATE
    src/main.c
    src/ws2812_gpio_sam0.c
)

