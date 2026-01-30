/*
 * LED Thread - NeoPixel color cycling based on CAN frames
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#define LOG_LEVEL 4
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(led);

#include <zephyr/kernel.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/device.h>
#include <zephyr/sys/util.h>

#include "canwork.h"

#define STRIP_NUM_PIXELS 1

/* LED thread stack size and priority (lowest priority) */
#define LED_THREAD_STACK_SIZE 1024
#define LED_THREAD_PRIORITY   K_LOWEST_APPLICATION_THREAD_PRIO

#define RGB(_r, _g, _b) { .r = (_r), .g = (_g), .b = (_b) }

static const struct led_rgb colors[] = {
	RGB(0x20, 0x00, 0x00), /* red */
	RGB(0x00, 0x20, 0x00), /* green */
	RGB(0x00, 0x00, 0x20), /* blue */
	RGB(0x20, 0x20, 0x20), /* white (RGB mix) */
};

static struct led_rgb pixels[STRIP_NUM_PIXELS];

static const struct device *const strip = DEVICE_DT_GET(DT_NODELABEL(led_strip));

/*
 * LED flashing thread - runs at lowest priority
 * This thread handles the LED color cycling independently
 */
static void led_thread_entry(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	size_t color = 0;
	int rc;

	LOG_INF("LED thread started (priority: %d)", LED_THREAD_PRIORITY);

	if (!device_is_ready(strip)) {
		LOG_ERR("LED strip device %s is NOT ready", strip->name);
		return;
	}

	LOG_INF("LED strip device is ready");
	LOG_INF("Starting color cycle based on CAN frames received");

	while (1) {
		/* Select color based on CAN frames received mod 4 */
		color = canwork_get_frames_processed() % ARRAY_SIZE(colors);

		memset(&pixels, 0x00, sizeof(pixels));
		memcpy(&pixels[0], &colors[color], sizeof(struct led_rgb));

		rc = led_strip_update_rgb(strip, pixels, STRIP_NUM_PIXELS);
		if (rc) {
			LOG_ERR("led_strip_update_rgb failed: %d", rc);
		}

		k_sleep(K_MSEC(50));
	}
}

/* Define LED thread statically - starts automatically at boot */
K_THREAD_DEFINE(led_thread, LED_THREAD_STACK_SIZE,
		led_thread_entry, NULL, NULL, NULL,
		LED_THREAD_PRIORITY, 0, 0);
