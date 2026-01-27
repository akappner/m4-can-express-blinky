/*
 * NeoPixel LED Strip + CAN Bus Demo for Adafruit Feather M4 CAN Express
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#define LOG_LEVEL 4
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main);

#include <zephyr/kernel.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/can.h>
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

/* Device handles */
static const struct device *const strip = DEVICE_DT_GET(DT_NODELABEL(led_strip));
static const struct device *const can_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_canbus));

/* GPIO for CAN transceiver control */
static const struct gpio_dt_spec boost_enable = GPIO_DT_SPEC_GET(DT_ALIAS(can_boost_enable), gpios);
static const struct gpio_dt_spec can_standby = GPIO_DT_SPEC_GET(DT_ALIAS(can_standby), gpios);

/* GPIO for NeoPixel power */
static const struct gpio_dt_spec neopixel_pwr = GPIO_DT_SPEC_GET(DT_NODELABEL(neopixel_power), gpios);

/* CAN receive thread stack */
#define CAN_RX_THREAD_STACK_SIZE 1024
#define CAN_RX_THREAD_PRIORITY 5

K_THREAD_STACK_DEFINE(can_rx_stack, CAN_RX_THREAD_STACK_SIZE);
static struct k_thread can_rx_thread_data;

/* CAN message queue */
CAN_MSGQ_DEFINE(can_msgq, 10);

static uint32_t rx_frame_count = 0;

/**
 * Enable the CAN transceiver hardware
 * - Set BOOST_ENABLE high to power the 5V boost converter
 * - Set CAN_STANDBY low to take transceiver out of standby
 */
static int can_transceiver_enable(void)
{
	int ret;

	/* Configure boost enable pin */
	if (!gpio_is_ready_dt(&boost_enable)) {
		LOG_ERR("Boost enable GPIO not ready");
		return -ENODEV;
	}

	ret = gpio_pin_configure_dt(&boost_enable, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		LOG_ERR("Failed to configure boost enable: %d", ret);
		return ret;
	}

	/* Configure standby pin */
	if (!gpio_is_ready_dt(&can_standby)) {
		LOG_ERR("CAN standby GPIO not ready");
		return -ENODEV;
	}

	/* Set standby LOW to enable transceiver (active low) */
	ret = gpio_pin_configure_dt(&can_standby, GPIO_OUTPUT_INACTIVE);
	if (ret < 0) {
		LOG_ERR("Failed to configure CAN standby: %d", ret);
		return ret;
	}

	LOG_INF("CAN transceiver enabled (boost=HIGH, standby=LOW)");
	return 0;
}

/**
 * CAN receive thread - listens for frames and logs them
 */
static void can_rx_thread(void *arg1, void *arg2, void *arg3)
{
	ARG_UNUSED(arg1);
	ARG_UNUSED(arg2);
	ARG_UNUSED(arg3);

	struct can_frame frame;

	LOG_INF("CAN RX thread started, waiting for frames...");

	while (1) {
		/* Wait for a CAN frame */
		if (k_msgq_get(&can_msgq, &frame, K_FOREVER) == 0) {
			rx_frame_count++;

			LOG_INF("CAN frame #%u: ID=0x%03X [%s] DLC=%d",
				rx_frame_count,
				frame.id,
				(frame.flags & CAN_FRAME_IDE) ? "EXT" : "STD",
				frame.dlc);

			/* Print data bytes */
			if (frame.dlc > 0) {
				char hex_buf[64];
				int pos = 0;
				for (int i = 0; i < frame.dlc && pos < sizeof(hex_buf) - 3; i++) {
					pos += snprintf(hex_buf + pos, sizeof(hex_buf) - pos,
							"%02X ", frame.data[i]);
				}
				LOG_INF("  Data: %s", hex_buf);
			}
		}
	}
}

/**
 * CAN receive callback - queues received frames
 */
static void can_rx_callback(const struct device *dev, struct can_frame *frame, void *user_data)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(user_data);

	/* Queue the frame for processing by the RX thread */
	if (k_msgq_put(&can_msgq, frame, K_NO_WAIT) != 0) {
		LOG_WRN("CAN RX queue full, frame dropped");
	}
}

/**
 * Initialize CAN bus at 500 kbit/s
 */
static int can_init(void)
{
	int ret;
	struct can_filter filter = {
		.flags = 0,  /* Accept all standard frames */
		.id = 0,
		.mask = 0,   /* Match any ID */
	};

	if (!device_is_ready(can_dev)) {
		LOG_ERR("CAN device not ready");
		return -ENODEV;
	}

	/* Set bitrate to 500 kbit/s */
	ret = can_set_bitrate(can_dev, 500000);
	if (ret < 0) {
		LOG_ERR("Failed to set CAN bitrate: %d", ret);
		return ret;
	}

	/* Add a filter to receive all frames */
	ret = can_add_rx_filter_msgq(can_dev, &can_msgq, &filter);
	if (ret < 0) {
		LOG_ERR("Failed to add CAN filter: %d", ret);
		return ret;
	}

	/* Start the CAN controller */
	ret = can_start(can_dev);
	if (ret < 0) {
		LOG_ERR("Failed to start CAN: %d", ret);
		return ret;
	}

	LOG_INF("CAN initialized at 500 kbit/s");

	/* Start the RX processing thread */
	k_thread_create(&can_rx_thread_data, can_rx_stack,
			K_THREAD_STACK_SIZEOF(can_rx_stack),
			can_rx_thread, NULL, NULL, NULL,
			CAN_RX_THREAD_PRIORITY, 0, K_NO_WAIT);
	k_thread_name_set(&can_rx_thread_data, "can_rx");

	return 0;
}

/**
 * Enable NeoPixel power
 */
static int neopixel_power_enable(void)
{
	int ret;

	if (!gpio_is_ready_dt(&neopixel_pwr)) {
		LOG_WRN("NeoPixel power GPIO not ready");
		return -ENODEV;
	}

	ret = gpio_pin_configure_dt(&neopixel_pwr, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		LOG_ERR("Failed to enable NeoPixel power: %d", ret);
		return ret;
	}

	LOG_INF("NeoPixel power enabled");
	return 0;
}

int main(void)
{
	size_t color = 0;
	int rc;

	/* Immediate marker - should appear right away */
	printk("\n*** main() started ***\n");

	/* USB is auto-initialized at boot (CONFIG_USB_DEVICE_INITIALIZE_AT_BOOT=y) */
	/* Brief delay to allow USB enumeration to complete */
	k_sleep(K_MSEC(1000));

	printk("\n=== Feather M4 CAN Express Demo ===\n");
	printk("NeoPixel + CAN Bus @ 500 kbit/s\n\n");

	/* Enable NeoPixel power */
	neopixel_power_enable();

	/* Enable CAN transceiver */
	rc = can_transceiver_enable();
	if (rc < 0) {
		LOG_ERR("Failed to enable CAN transceiver");
	}

	/* Initialize CAN */
	rc = can_init();
	if (rc < 0) {
		LOG_ERR("Failed to initialize CAN");
	}

	/* Check LED strip device */
	printk("Strip device: %s\n", strip->name);

	if (device_is_ready(strip)) {
		LOG_INF("LED strip device is ready");
	} else {
		LOG_ERR("LED strip device %s is NOT ready", strip->name);
	}

	LOG_INF("Starting color cycle: Red -> Green -> Blue -> White");
	LOG_INF("CAN RX active - send frames to see them logged");

	while (1) {
		memset(&pixels, 0x00, sizeof(pixels));
		memcpy(&pixels[0], &colors[color], sizeof(struct led_rgb));

		LOG_INF("Color %zu: R=%02x G=%02x B=%02x (CAN frames: %u)",
			color, pixels[0].r, pixels[0].g, pixels[0].b, rx_frame_count);

		rc = led_strip_update_rgb(strip, pixels, STRIP_NUM_PIXELS);
		if (rc) {
			LOG_ERR("led_strip_update_rgb failed: %d", rc);
		}

		k_sleep(K_MSEC(1000));

		color = (color + 1) % ARRAY_SIZE(colors);
	}

	return 0;
}
