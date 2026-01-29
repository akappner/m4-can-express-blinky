/*
 * PWM Threads - Hardware PWM control on two pins
 *
 * PWM0: TCC0 channel 2 on PA22
 * PWM1: TC0 channel 1 on PA5
 *
 * Both cycle through 0%, 50%, 100% duty cycle (5 seconds each)
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_LEVEL 4
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(pwm);

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/pwm.h>

/* PWM thread stack size and priority */
#define PWM_THREAD_STACK_SIZE 1024
#define PWM_THREAD_PRIORITY   8

/* PWM period (1 kHz = 1ms period) */
#define PWM_PERIOD_NS 1000000U

/* Duty cycle hold time */
#define DUTY_HOLD_MS 5000

/* PWM configuration structure - device resolved at runtime */
struct pwm_config {
	const char *dev_label;
	uint32_t channel;
	const char *name;
};

/* PWM configurations - device lookup done at runtime */
static const struct pwm_config pwm_configs[] = {
	{
		.dev_label = "pwm_0",  /* DT alias */
		.channel = 3,         /* TCC0_WO3 on PA23 (LED) */
		.name = "PWM0",
	},
	{
		.dev_label = "pwm_1",  /* DT alias */
		.channel = 1,         /* TC0_WO1 */
		.name = "PWM1",
	},
};

/* Duty cycle percentages to cycle through */
static const uint8_t duty_cycles[] = { 0, 50, 100 };

/*
 * PWM thread entry point - shared by both PWM threads
 * p1: index into pwm_configs
 */
static void pwm_thread_entry(void *p1, void *p2, void *p3)
{
	int cfg_idx = (int)(intptr_t)p1;
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	const struct pwm_config *cfg = &pwm_configs[cfg_idx];
	const struct device *dev;
	size_t idx = 0;
	int ret;

	LOG_INF("%s thread started", cfg->name);

	/* Get PWM device at runtime */
	if (cfg_idx == 0) {
		dev = DEVICE_DT_GET(DT_ALIAS(pwm_0));
	} else {
		dev = DEVICE_DT_GET(DT_ALIAS(pwm_1));
	}

	if (dev == NULL) {
		LOG_ERR("%s: PWM device is NULL", cfg->name);
		return;
	}

	if (!device_is_ready(dev)) {
		LOG_ERR("%s: PWM device not ready", cfg->name);
		return;
	}

	LOG_INF("%s: PWM device ready (channel %u), period=%u ns",
		cfg->name, cfg->channel, PWM_PERIOD_NS);

	while (1) {
		uint8_t duty_pct = duty_cycles[idx];
		uint32_t pulse_ns = (PWM_PERIOD_NS * duty_pct) / 100;

		ret = pwm_set(dev, cfg->channel, PWM_PERIOD_NS, pulse_ns, 0);
		if (ret < 0) {
			LOG_ERR("%s: pwm_set failed: %d", cfg->name, ret);
		} else {
			LOG_INF("%s: duty=%3u%% (pulse=%u ns)",
				cfg->name, duty_pct, pulse_ns);
		}

		k_sleep(K_MSEC(DUTY_HOLD_MS));

		idx = (idx + 1) % ARRAY_SIZE(duty_cycles);
	}
}

/* Define PWM threads statically - delayed start (1000ms) to let system stabilize */
K_THREAD_DEFINE(pwm0_thread, PWM_THREAD_STACK_SIZE,
		pwm_thread_entry, (void *)0, NULL, NULL,
		PWM_THREAD_PRIORITY, 0, 1000);

/* TEMPORARILY DISABLED FOR DEBUGGING
K_THREAD_DEFINE(pwm1_thread, PWM_THREAD_STACK_SIZE,
		pwm_thread_entry, (void *)1, NULL, NULL,
		PWM_THREAD_PRIORITY, 0, 0);
*/
