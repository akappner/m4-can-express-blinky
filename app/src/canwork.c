/*
 * CAN Work Thread - Processes received CAN frames
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_LEVEL 4
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(canwork);

#include <zephyr/kernel.h>
#include <zephyr/drivers/can.h>

#include "canwork.h"

/* Thread stack size and priority */
#define CANWORK_THREAD_STACK_SIZE 1024
#define CANWORK_THREAD_PRIORITY   6

/* Work queue depth - frames dropped if full */
#define CAN_WORK_QUEUE_DEPTH 16

/* Message queue for canrx -> canwork communication */
K_MSGQ_DEFINE(can_work_msgq, sizeof(struct can_frame), CAN_WORK_QUEUE_DEPTH, 4);

/* Statistics */
static volatile uint32_t frames_processed;
static volatile uint32_t frames_dropped;

int canwork_submit(const struct can_frame *frame)
{
	int ret;

	ret = k_msgq_put(&can_work_msgq, frame, K_NO_WAIT);
	if (ret != 0) {
		frames_dropped++;
		LOG_WRN("Work queue full, frame dropped (total dropped: %u)",
			frames_dropped);
		return -ENOMSG;
	}

	return 0;
}

/*
 * CAN work thread - processes queued frames
 */
static void canwork_thread_entry(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	struct can_frame frame;
	int ret;

	LOG_INF("CAN work thread started");

	while (1) {
		/* Wait for a frame to process */
		ret = k_msgq_get(&can_work_msgq, &frame, K_FOREVER);
		if (ret == 0) {
			frames_processed++;

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

/* Define CAN work thread statically */
K_THREAD_DEFINE(canwork_thread, CANWORK_THREAD_STACK_SIZE,
		canwork_thread_entry, NULL, NULL, NULL,
		CANWORK_THREAD_PRIORITY, 0, 0);
