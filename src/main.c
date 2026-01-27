/*
 * NeoPixel LED Strip Demo for Adafruit Feather M4 Express CAN
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#define LOG_LEVEL 4
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main);

#include <zephyr/kernel.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/device.h>
#include <zephyr/sys/util.h>

#define STRIP_NUM_PIXELS 1

#define RGB(_r, _g, _b) { .r = (_r), .g = (_g), .b = (_b) }

static const struct led_rgb colors[] = {
	RGB(0x20, 0x00, 0x00), /* red */
	RGB(0x00, 0x20, 0x00), /* green */
	RGB(0x00, 0x00, 0x20), /* blue */
	RGB(0x20, 0x20, 0x20), /* white (RGB mix) */
};

static struct led_rgb pixels[STRIP_NUM_PIXELS];

static const struct device *const strip = DEVICE_DT_GET(DT_NODELABEL(led_strip));

int main(void)
{
	size_t color = 0;
	int rc;

	/* Immediate marker - should appear right away */
	printk("\n*** main() started ***\n");

	/* USB is auto-initialized at boot (CONFIG_USB_DEVICE_INITIALIZE_AT_BOOT=y) */
	/* Brief delay to allow USB enumeration to complete */
	k_sleep(K_MSEC(1000));

	printk("\n=== NeoPixel LED Strip Demo ===\n");
	printk("Strip device: %s\n", strip->name);

	if (device_is_ready(strip)) {
		LOG_INF("LED strip device is ready");
	} else {
		LOG_ERR("LED strip device %s is NOT ready", strip->name);
		return 0;
	}

	LOG_INF("Starting color cycle: Red -> Green -> Blue -> White");

	while (1) {
		memset(&pixels, 0x00, sizeof(pixels));
		memcpy(&pixels[0], &colors[color], sizeof(struct led_rgb));

		LOG_INF("Color %zu: R=%02x G=%02x B=%02x",
			color, pixels[0].r, pixels[0].g, pixels[0].b);

		rc = led_strip_update_rgb(strip, pixels, STRIP_NUM_PIXELS);
		if (rc) {
			LOG_ERR("led_strip_update_rgb failed: %d", rc);
		}

		k_sleep(K_MSEC(1000));

		color = (color + 1) % ARRAY_SIZE(colors);
	}

	return 0;
}

