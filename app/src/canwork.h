/*
 * CAN Work Thread Header
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef CANWORK_H_
#define CANWORK_H_

#include <zephyr/kernel.h>
#include <zephyr/drivers/can.h>

/* Work queue for CAN frames to be processed */
extern struct k_msgq can_work_msgq;

/*
 * Submit a CAN frame for processing.
 * Non-blocking - returns 0 on success, -ENOMSG if queue is full (frame dropped).
 */
int canwork_submit(const struct can_frame *frame);

#endif /* CANWORK_H_ */
