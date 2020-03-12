/*
 * Copyright (c) 2018-2019 Peter Bigot Consulting, LLC
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include <zephyr.h>
#include <init.h>
#include <drivers/gpio.h>
#include <drivers/adc.h>
#include <drivers/sensor.h>
#include <logging/log.h>

#include "battery.h"

LOG_MODULE_REGISTER(BATTERY, CONFIG_ADC_LOG_LEVEL);

#define BATTERY_ADC_GAIN ADC_GAIN_1_3

struct io_channel_config {
	const char *label;
	u8_t channel;
};

struct gpio_channel_config {
	const char *label;
	u8_t pin;
	u8_t flags;
};

struct divider_config {
	const struct io_channel_config io_channel;
	const struct gpio_channel_config power_gpios;
	const u32_t output_ohm;
	const u32_t full_ohm;
};

static const struct divider_config divider_config = {
	.io_channel = DT_VOLTAGE_DIVIDER_VBATT_IO_CHANNELS,
#ifdef DT_VOLTAGE_DIVIDER_VBATT_POWER_GPIOS
	.power_gpios = DT_VOLTAGE_DIVIDER_VBATT_POWER_GPIOS,
#endif
	.output_ohm = DT_VOLTAGE_DIVIDER_VBATT_OUTPUT_OHMS,
	.full_ohm = DT_VOLTAGE_DIVIDER_VBATT_FULL_OHMS,
};

struct divider_data {
	struct device *adc_device;
	struct device *gpio_device;
	struct adc_channel_cfg adc_config;
	struct adc_sequence adc_sequence;
	s16_t raw;
};
static struct divider_data divider_data;

static int divider_setup(void)
{
	const struct divider_config *config = &divider_config;
	const struct io_channel_config *io_channel = &config->io_channel;
	const struct gpio_channel_config *gpio_config = &config->power_gpios;
	struct adc_sequence *sequence = &divider_data.adc_sequence;
	struct adc_channel_cfg *channel_cfg = &divider_data.adc_config;
	int rc;

	divider_data.adc_device = device_get_binding(io_channel->label);
	if (divider_data.adc_device == NULL) {
		LOG_ERR("Failed to get ADC %s", io_channel->label);
		return -ENOENT;
	}

	if (gpio_config->label) {
		divider_data.gpio_device = device_get_binding(gpio_config->label);
		if (divider_data.gpio_device == NULL) {
			LOG_ERR("Failed to get GPIO %s", gpio_config->label);
			return -ENOENT;
		}
		rc = gpio_pin_configure(divider_data.gpio_device, gpio_config->pin,
					GPIO_OUTPUT_INACTIVE | gpio_config->flags);
		if (rc != 0) {
			LOG_ERR("Failed to control feed %s.%u: %d",
				gpio_config->label, gpio_config->pin, rc);
			return rc;
		}
	}

#ifdef CONFIG_ADC_NRFX_SAADC
	*sequence = (struct adc_sequence){
		.channels = BIT(0),
		.buffer = &divider_data->raw,
		.buffer_size = sizeof(divider_data->raw),
		.oversampling = 4,
		.calibrate = true,
	};

	*channel_cfg = (struct adc_channel_cfg){
		.gain = BATTERY_ADC_GAIN,
		.reference = ADC_REF_INTERNAL,
		.acquisition_time = ADC_ACQ_TIME(ADC_ACQ_TIME_MICROSECONDS, 40),
		.input_positive = SAADC_CH_PSELP_PSELP_AnalogInput0 + iocp->channel,
	};

	sequence->resolution = 14;
#elif defined(CONFIG_ADC_NRFX_ADC)
	*sequence = (struct adc_sequence){
		.channels = BIT(0),
		.buffer = &divider_data.raw,
		.buffer_size = sizeof(divider_data.raw),
		.oversampling = 0,
		.calibrate = true,
	    .resolution = 10,
	};

	*channel_cfg = (struct adc_channel_cfg){
        .acquisition_time = ADC_ACQ_TIME_DEFAULT,
		.gain = BATTERY_ADC_GAIN,
		.reference = ADC_REF_INTERNAL,
        .channel_id = 0,
#if CONFIG_ADC_CONFIGURABLE_INPUTS
		.input_positive = BIT(io_channel->channel), // Why is this a bit value?
#endif
	};
#else /* CONFIG_ADC_var */
#error Unsupported ADC
#endif /* CONFIG_ADC_var */

	rc = adc_channel_setup(divider_data.adc_device, channel_cfg);
    Z_LOG(rc < 0 ? LOG_LEVEL_ERR : LOG_LEVEL_INF, "Setup AIN%u got %d", io_channel->channel, rc);

	return rc;
}

static bool battery_ok;

static int battery_setup(struct device *arg)
{
	int rc = divider_setup();

	battery_ok = (rc == 0);
	LOG_INF("Battery setup: %d %d", rc, battery_ok);
	return rc;
}

SYS_INIT(battery_setup, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

int battery_measure_enable(bool enable)
{
	int rc = -ENOENT;

	if (battery_ok) {
		const struct divider_data *data = &divider_data;
		const struct gpio_channel_config *gpio_config = &divider_config.power_gpios;

		rc = 0;
		if (data->gpio_device) {
			rc = gpio_pin_set(data->gpio_device, gpio_config->pin, enable);
		}
	}
	return rc;
}

int battery_sample(void)
{
	int rc = -ENOENT;

	if (battery_ok) {
		struct divider_data *data = &divider_data;
		const struct divider_config *config = &divider_config;
		struct adc_sequence *sequence = &data->adc_sequence;

		rc = adc_read(data->adc_device, sequence);
		sequence->calibrate = false;
		if (rc == 0) {
			s32_t val = data->raw;

			adc_raw_to_millivolts(adc_ref_internal(data->adc_device),
					      data->adc_config.gain,
					      sequence->resolution,
					      &val);
			rc = val * (u64_t)config->full_ohm / config->output_ohm;
			LOG_INF("raw %u ~ %u mV => %d mV\n",
				data->raw, val, rc);
		}
	}

	return rc;
}

unsigned int battery_level_pptt(unsigned int batt_mV,
				const struct battery_level_point *curve)
{
	const struct battery_level_point *pb = curve;

	if (batt_mV >= pb->lvl_mV) {
		/* Measured voltage above highest point, cap at maximum. */
		return pb->lvl_pptt;
	}
	/* Go down to the last point at or below the measured voltage. */
	while ((pb->lvl_pptt > 0)
	       && (batt_mV < pb->lvl_mV)) {
		++pb;
	}
	if (batt_mV < pb->lvl_mV) {
		/* Below lowest point, cap at minimum */
		return pb->lvl_pptt;
	}

	/* Linear interpolation between below and above points. */
	const struct battery_level_point *pa = pb - 1;

	return pb->lvl_pptt
	       + ((pa->lvl_pptt - pb->lvl_pptt)
		  * (batt_mV - pb->lvl_mV)
		  / (pa->lvl_mV - pb->lvl_mV));
}
