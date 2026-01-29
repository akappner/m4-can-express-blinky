/*
 * NeoPixel LED Strip Demo for Adafruit Feather M4 Express CAN
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <string.h>

#define LOG_LEVEL 4
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main);

#include <zephyr/kernel.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/device.h>
#include <zephyr/sys/util.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/drivers/uart.h>

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
	uint32_t dtr = 0;

	/* Enable USB */
	const struct device *usb_dev = DEVICE_DT_GET_ONE(zephyr_cdc_acm_uart);

	if (!device_is_ready(usb_dev)) {
		LOG_ERR("USB device not ready");
		return 0;
	}

	rc = usb_enable(NULL);
	if (rc != 0 && rc != -EALREADY) {
		LOG_ERR("Failed to enable USB: %d", rc);
		return 0;
	}

	/* Wait for DTR (terminal connected) - with timeout */
	LOG_INF("Waiting for USB connection...");
	int wait_count = 0;
	while (!dtr && wait_count < 100) {
		uart_line_ctrl_get(usb_dev, UART_LINE_CTRL_DTR, &dtr);
		k_sleep(K_MSEC(100));
		wait_count++;
	}

	if (dtr) {
		LOG_INF("USB connected!");
	} else {
		LOG_INF("USB timeout, continuing anyway...");
	}

	/* Give terminal time to start receiving */
	k_sleep(K_MSEC(500));

	LOG_INF("=== NeoPixel LED Strip Demo ===");
	LOG_INF("Strip device: %s", strip->name);

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

