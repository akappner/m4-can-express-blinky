/*
 * Main - USB initialization and system startup
 *
 * Adafruit Feather M4 Express CAN Demo
 * - LED thread: cycles NeoPixel through colors
 * - CAN RX thread: receives CAN frames and prints to console
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>

#define LOG_LEVEL 4
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main);

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/usb/usb_device.h>

int main(void)
{
	int rc;

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

	LOG_INF("=== Feather M4 CAN Express Demo ===");
	LOG_INF("LED thread: NeoPixel color cycling");
	LOG_INF("CAN RX thread: listening for CAN frames");
	LOG_INF("CAN work thread: processing received frames");
	LOG_INF("PWM0 thread: PA23/D13 LED duty cycle sweep");
	LOG_INF("PWM1 thread: PA5 duty cycle sweep");

	return 0;
}
