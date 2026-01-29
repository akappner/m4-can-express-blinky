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
#define PWM_THREAD_STACK_SIZE 512
#define PWM_THREAD_PRIORITY   8

/* PWM period (1 kHz = 1ms period) */
#define PWM_PERIOD_NS 1000000U

/* Duty cycle hold time */
#define DUTY_HOLD_MS 5000

/* PWM configuration structure */
struct pwm_config {
	const struct device *dev;
	uint32_t channel;
	const char *name;
};

/* PWM devices from devicetree aliases */
static const struct pwm_config pwm_configs[] = {
	{
		.dev = DEVICE_DT_GET(DT_ALIAS(pwm_0)),
		.channel = 2,  /* TCC0_WO2 */
		.name = "PWM0",
	},
	{
		.dev = DEVICE_DT_GET(DT_ALIAS(pwm_1)),
		.channel = 1,  /* TC0_WO1 */
		.name = "PWM1",
	},
};

/* Duty cycle percentages to cycle through */
static const uint8_t duty_cycles[] = { 0, 50, 100 };

/*
 * PWM thread entry point - shared by both PWM threads
 * p1: pointer to pwm_config
 */
static void pwm_thread_entry(void *p1, void *p2, void *p3)
{
	const struct pwm_config *cfg = (const struct pwm_config *)p1;
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	size_t idx = 0;
	int ret;

	LOG_INF("%s thread started (channel %u)", cfg->name, cfg->channel);

	if (!device_is_ready(cfg->dev)) {
		LOG_ERR("%s: PWM device not ready", cfg->name);
		return;
	}

	LOG_INF("%s: PWM device ready, period=%u ns", cfg->name, PWM_PERIOD_NS);

	while (1) {
		uint8_t duty_pct = duty_cycles[idx];
		uint32_t pulse_ns = (PWM_PERIOD_NS * duty_pct) / 100;

		ret = pwm_set(cfg->dev, cfg->channel, PWM_PERIOD_NS, pulse_ns, 0);
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

/* Define PWM threads statically - start automatically at boot */
/* TEMPORARILY DISABLED FOR DEBUGGING
K_THREAD_DEFINE(pwm0_thread, PWM_THREAD_STACK_SIZE,
		pwm_thread_entry, (void *)&pwm_configs[0], NULL, NULL,
		PWM_THREAD_PRIORITY, 0, 0);

K_THREAD_DEFINE(pwm1_thread, PWM_THREAD_STACK_SIZE,
		pwm_thread_entry, (void *)&pwm_configs[1], NULL, NULL,
		PWM_THREAD_PRIORITY, 0, 0);
*/
