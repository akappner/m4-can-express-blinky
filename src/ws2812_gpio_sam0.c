/*
 * WS2812 GPIO driver for SAM0 (SAMD51, etc.)
 *
 * Bit-banged WS2812/NeoPixel driver optimized for SAMD51 at 120 MHz.
 * Based on the nRF GPIO driver, adapted for SAM0 register layout and timing.
 *
 * Features:
 *   - Direct register access for precise timing
 *   - Optional power GPIO support (for boards like Feather M4 CAN)
 *   - Automatic PMUX disable for GPIO mode
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/drivers/led_strip.h>
#include <string.h>

#define LOG_LEVEL CONFIG_LED_STRIP_LOG_LEVEL
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(ws2812_gpio_sam0);

#include <zephyr/kernel.h>
#include <soc.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/device.h>
#include <zephyr/dt-bindings/led/led.h>

/* Check if this is a SAM0 device */
#if !defined(PORT) && !defined(PortGroup)
#error "This driver is only for SAM0 devices (SAMD51, etc.)"
#endif

struct ws2812_gpio_sam0_cfg {
	struct gpio_dt_spec gpio;
	struct gpio_dt_spec power_gpio;
	bool has_power_gpio;
	uint8_t num_colors;
	const uint8_t *color_mapping;
	size_t length;
};

/*
 * SAM0 GPIO register layout for PortGroup:
 *   OUTCLR at offset 0x14
 *   OUTSET at offset 0x18
 *
 * We use OUTCLR as base, so:
 *   base + 0 = OUTCLR (set low)
 *   base + 4 = OUTSET (set high)
 */
#define SET_HIGH "str %[p], [%[r], #4]\n"
#define SET_LOW "str %[p], [%[r], #0]\n"

/*
 * Timing for WS2812 at 120 MHz (SAMD51):
 * Each NOP = 1 cycle = 8.33ns
 *
 * WS2812 timing requirements (from datasheet):
 *   T0H: 400ns ± 150ns  -> target ~48 cycles
 *   T0L: 850ns ± 150ns  -> target ~102 cycles
 *   T1H: 800ns ± 150ns  -> target ~96 cycles
 *   T1L: 450ns ± 150ns  -> target ~54 cycles
 */
#ifndef WS2812_SAM0_DELAY_T1H
#define WS2812_SAM0_DELAY_T1H 90
#endif
#ifndef WS2812_SAM0_DELAY_T1L
#define WS2812_SAM0_DELAY_T1L 45
#endif
#ifndef WS2812_SAM0_DELAY_T0H
#define WS2812_SAM0_DELAY_T0H 40
#endif
#ifndef WS2812_SAM0_DELAY_T0L
#define WS2812_SAM0_DELAY_T0L 95
#endif

#define NOPS(i, _) "nop\n"
#define NOP_N_TIMES(n) LISTIFY(n, NOPS, ())

/* Send out a 1 bit's pulse */
#define ONE_BIT(base, pin) do {                     \
	__asm volatile (SET_HIGH                    \
			NOP_N_TIMES(WS2812_SAM0_DELAY_T1H)   \
			SET_LOW                         \
			NOP_N_TIMES(WS2812_SAM0_DELAY_T1L)   \
			::                              \
			[r] "l" (base),                 \
			[p] "l" (pin)); } while (false)

/* Send out a 0 bit's pulse */
#define ZERO_BIT(base, pin) do {                    \
	__asm volatile (SET_HIGH                    \
			NOP_N_TIMES(WS2812_SAM0_DELAY_T0H)   \
			SET_LOW                         \
			NOP_N_TIMES(WS2812_SAM0_DELAY_T0L)   \
			::                              \
			[r] "l" (base),                 \
			[p] "l" (pin)); } while (false)

/*
 * Get PortGroup pointer for a given GPIO port device.
 */
static inline PortGroup *get_port_group(const struct device *gpio_dev)
{
	const char *name = gpio_dev->name;

	if (name[4] == 'a' || name[4] == 'A') {
		return &PORT->Group[0];
	} else if (name[4] == 'b' || name[4] == 'B') {
		return &PORT->Group[1];
	} else if (name[4] == 'c' || name[4] == 'C') {
		return &PORT->Group[2];
	} else if (name[4] == 'd' || name[4] == 'D') {
		return &PORT->Group[3];
	}

	return &PORT->Group[1];
}

static int send_buf(const struct ws2812_gpio_sam0_cfg *config,
		    uint8_t *buf, size_t len)
{
	PortGroup *port = get_port_group(config->gpio.port);
	volatile uint32_t *base = &port->OUTCLR.reg;
	const uint32_t val = BIT(config->gpio.pin);
	unsigned int key;

	port->OUTCLR.reg = val;
	k_busy_wait(80);

	key = irq_lock();

	while (len--) {
		uint32_t b = *buf++;
		int32_t i;

		for (i = 7; i >= 0; i--) {
			if (b & BIT(i)) {
				ONE_BIT(base, val);
			} else {
				ZERO_BIT(base, val);
			}
		}
	}

	irq_unlock(key);
	port->OUTCLR.reg = val;

	return 0;
}

static int ws2812_gpio_sam0_update_rgb(const struct device *dev,
				       struct led_rgb *pixels,
				       size_t num_pixels)
{
	const struct ws2812_gpio_sam0_cfg *config = dev->config;
	uint8_t *ptr = (uint8_t *)pixels;
	size_t i;

	for (i = 0; i < num_pixels; i++) {
		uint8_t j;
		const struct led_rgb current_pixel = pixels[i];

		for (j = 0; j < config->num_colors; j++) {
			switch (config->color_mapping[j]) {
			case LED_COLOR_ID_WHITE:
				*ptr++ = 0;
				break;
			case LED_COLOR_ID_RED:
				*ptr++ = current_pixel.r;
				break;
			case LED_COLOR_ID_GREEN:
				*ptr++ = current_pixel.g;
				break;
			case LED_COLOR_ID_BLUE:
				*ptr++ = current_pixel.b;
				break;
			default:
				return -EINVAL;
			}
		}
	}

	return send_buf(config, (uint8_t *)pixels, num_pixels * config->num_colors);
}

static size_t ws2812_gpio_sam0_length(const struct device *dev)
{
	const struct ws2812_gpio_sam0_cfg *config = dev->config;

	return config->length;
}

static const struct led_strip_driver_api ws2812_gpio_sam0_api = {
	.update_rgb = ws2812_gpio_sam0_update_rgb,
	.length = ws2812_gpio_sam0_length,
};

static int ws2812_gpio_sam0_init(const struct device *dev)
{
	const struct ws2812_gpio_sam0_cfg *cfg = dev->config;
	PortGroup *port;
	PortGroup *power_port;
	uint8_t i;
	uint8_t pin = cfg->gpio.pin;
	int ret;

	/*
	 * Enable power to the NeoPixel if power-gpios is defined.
	 * Required on boards like Feather M4 CAN where the NeoPixel
	 * has a separate power control pin.
	 */
	if (cfg->has_power_gpio) {
		if (!gpio_is_ready_dt(&cfg->power_gpio)) {
			LOG_ERR("Power GPIO device not ready");
			return -ENODEV;
		}

		power_port = get_port_group(cfg->power_gpio.port);
		power_port->PINCFG[cfg->power_gpio.pin].reg &= ~PORT_PINCFG_PMUXEN;

		ret = gpio_pin_configure_dt(&cfg->power_gpio, GPIO_OUTPUT_HIGH);
		if (ret < 0) {
			LOG_ERR("Failed to configure power GPIO: %d", ret);
			return ret;
		}

		LOG_DBG("NeoPixel power enabled on port %s pin %d",
			cfg->power_gpio.port->name, cfg->power_gpio.pin);

		k_msleep(10);
	}

	if (!gpio_is_ready_dt(&cfg->gpio)) {
		LOG_ERR("GPIO device not ready");
		return -ENODEV;
	}

	for (i = 0; i < cfg->num_colors; i++) {
		switch (cfg->color_mapping[i]) {
		case LED_COLOR_ID_WHITE:
		case LED_COLOR_ID_RED:
		case LED_COLOR_ID_GREEN:
		case LED_COLOR_ID_BLUE:
			break;
		default:
			LOG_ERR("%s: invalid channel to color mapping", dev->name);
			return -EINVAL;
		}
	}

	port = get_port_group(cfg->gpio.port);

	/*
	 * Clear PMUXEN to disconnect pin from any peripheral (SERCOM, etc.)
	 * and enable GPIO mode.
	 */
	port->PINCFG[pin].reg &= ~PORT_PINCFG_PMUXEN;

	ret = gpio_pin_configure_dt(&cfg->gpio, GPIO_OUTPUT_LOW);
	if (ret < 0) {
		LOG_ERR("Failed to configure GPIO: %d", ret);
		return ret;
	}

	port->DIRSET.reg = BIT(pin);
	port->OUTCLR.reg = BIT(pin);

	LOG_DBG("WS2812 SAM0 GPIO driver initialized on port %s pin %d",
		cfg->gpio.port->name, pin);

	return 0;
}

/* Color mapping for RGBW NeoPixels (GRBW wire order) */
static const uint8_t ws2812_sam0_color_mapping[] = {
	LED_COLOR_ID_GREEN,
	LED_COLOR_ID_RED,
	LED_COLOR_ID_BLUE,
	LED_COLOR_ID_WHITE,
};

#define HAS_POWER_GPIO DT_NODE_HAS_PROP(DT_NODELABEL(led_strip), power_gpios)

static const struct ws2812_gpio_sam0_cfg ws2812_gpio_sam0_cfg = {
	.gpio = GPIO_DT_SPEC_GET(DT_NODELABEL(led_strip), gpios),
#if HAS_POWER_GPIO
	.power_gpio = GPIO_DT_SPEC_GET(DT_NODELABEL(led_strip), power_gpios),
	.has_power_gpio = true,
#else
	.has_power_gpio = false,
#endif
	.num_colors = 4,
	.color_mapping = ws2812_sam0_color_mapping,
	.length = DT_PROP(DT_NODELABEL(led_strip), chain_length),
};

DEVICE_DT_DEFINE(DT_NODELABEL(led_strip),
		 ws2812_gpio_sam0_init,
		 NULL,
		 NULL,
		 &ws2812_gpio_sam0_cfg,
		 POST_KERNEL,
		 CONFIG_LED_STRIP_INIT_PRIORITY,
		 &ws2812_gpio_sam0_api);

