/*
 * CAN RX Thread - Receives CAN frames and prints to console
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_LEVEL 4
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(canrx);

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/can.h>
#include <zephyr/drivers/gpio.h>

/* CAN RX thread stack size and priority */
#define CANRX_THREAD_STACK_SIZE 1024
#define CANRX_THREAD_PRIORITY   7

/* CAN device from devicetree */
static const struct device *const can_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_canbus));

/* CAN transceiver control GPIOs */
static const struct gpio_dt_spec boost_enable = GPIO_DT_SPEC_GET(DT_NODELABEL(boost_enable), gpios);
static const struct gpio_dt_spec can_standby = GPIO_DT_SPEC_GET(DT_NODELABEL(can_standby), gpios);

/* Message queue for received frames */
CAN_MSGQ_DEFINE(can_msgq, 16);

static void can_rx_callback(const struct device *dev, struct can_frame *frame, void *user_data)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(user_data);

	/* Put frame in message queue for processing by thread */
	k_msgq_put(&can_msgq, frame, K_NO_WAIT);
}

static int can_transceiver_enable(void)
{
	int ret;

	/* Configure boost enable GPIO (HIGH to enable 5V for CAN bus) */
	if (!gpio_is_ready_dt(&boost_enable)) {
		LOG_ERR("Boost enable GPIO not ready");
		return -ENODEV;
	}

	ret = gpio_pin_configure_dt(&boost_enable, GPIO_OUTPUT_HIGH);
	if (ret < 0) {
		LOG_ERR("Failed to configure boost enable: %d", ret);
		return ret;
	}

	/* Configure CAN standby GPIO (LOW to enable transceiver) */
	if (!gpio_is_ready_dt(&can_standby)) {
		LOG_ERR("CAN standby GPIO not ready");
		return -ENODEV;
	}

	ret = gpio_pin_configure_dt(&can_standby, GPIO_OUTPUT_LOW);
	if (ret < 0) {
		LOG_ERR("Failed to configure CAN standby: %d", ret);
		return ret;
	}

	LOG_INF("CAN transceiver enabled");
	return 0;
}

/*
 * CAN RX thread - receives CAN frames and logs them
 */
static void canrx_thread_entry(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	struct can_frame frame;
	struct can_filter filter = {
		.flags = 0,
		.id = 0,
		.mask = 0,  /* Accept all IDs */
	};
	int ret;
	int filter_id;

	LOG_INF("CAN RX thread started");

	if (!device_is_ready(can_dev)) {
		LOG_ERR("CAN device not ready");
		return;
	}

	/* Enable the CAN transceiver */
	ret = can_transceiver_enable();
	if (ret < 0) {
		LOG_ERR("Failed to enable CAN transceiver");
		return;
	}

	/* Start the CAN controller */
	ret = can_start(can_dev);
	if (ret < 0) {
		LOG_ERR("Failed to start CAN: %d", ret);
		return;
	}

	/* Add filter to receive all frames */
	filter_id = can_add_rx_filter_msgq(can_dev, &can_msgq, &filter);
	if (filter_id < 0) {
		LOG_ERR("Failed to add CAN filter: %d", filter_id);
		return;
	}

	LOG_INF("CAN initialized, listening for frames...");

	while (1) {
		/* Wait for a frame */
		ret = k_msgq_get(&can_msgq, &frame, K_FOREVER);
		if (ret == 0) {
			/* Print the frame */
			if (frame.flags & CAN_FRAME_IDE) {
				LOG_INF("CAN RX: ID=0x%08x [%d]", frame.id, frame.dlc);
			} else {
				LOG_INF("CAN RX: ID=0x%03x [%d]", frame.id, frame.dlc);
			}

			/* Print data bytes */
			if (frame.dlc > 0) {
				LOG_HEXDUMP_INF(frame.data, frame.dlc, "Data:");
			}
		}
	}
}

/* Define CAN RX thread statically - starts automatically at boot */
K_THREAD_DEFINE(canrx_thread, CANRX_THREAD_STACK_SIZE,
		canrx_thread_entry, NULL, NULL, NULL,
		CANRX_THREAD_PRIORITY, 0, 0);
