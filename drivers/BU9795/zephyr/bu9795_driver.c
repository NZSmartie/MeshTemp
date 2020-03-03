/*
 * Copyright (c) 2019 Nordic Semiconductor
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bu9795_driver.h"
#include <zephyr/types.h>

#include <devicetree.h>
#include <drivers/gpio.h>
#include <drivers/spi.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(bme680, CONFIG_BU9795_LOG_LEVEL);

#define BU9795_BUS_NAME DT_INST_0_ROHM_BU9795_BUS_NAME

#define BU9795_CMD_DATA_BIT	BIT(7)

#define BU9795_CMD_ADDRESS			0x00
#define BU9795_CMD_MODE				0x40
#define BU9795_CMD_DISPLAY_CONTROL	0x20
#define BU9795_CMD_IC				0x68
#define BU9795_CMD_BLINK			0x70
#define BU9795_CMD_ALL_PIXEL		0x7C

#define DISPLAY_MODE_BIT	BIT(3)
#define DISPLAY_ON			0x04
#define DISPLAY_OFF 		0x00

#define DISPLAY_BIAS_LEVEL_BIT	BIT(2)
#define DISPLAY_BIAS_LEVEL_1_3	0x00
#define DISPLAY_BIAS_LEVEL_1_2	0x02

#define BU9795_ADDRESS_MASK	0x1F

#define BU9795_DISPLAY_FREQ_MASK	(BIT(4) | BIT(3))
#define BU9795_DISPLAY_FREQ_80		0x00
#define BU9795_DISPLAY_FREQ_71		0x04
#define BU9795_DISPLAY_FREQ_64		0x08
#define BU9795_DISPLAY_FREQ_53		0x0C

#define BU9795_DISPLAY_WAVEFORM_MASK	BIT(2)
#define BU9795_DISPLAY_WAVEFORM_LINE	0x00
#define BU9795_DISPLAY_WAVEFORM_FRAME	0x04

#define BU9795_DISPLAY_POWER_MASK	(BIT(1) | BIT(0))
#define BU9795_DISPLAY_POWER_SAVE_1	0x00
#define BU9795_DISPLAY_POWER_SAVE_2	0x01
#define BU9795_DISPLAY_POWER_NORMAL	0x02
#define BU9795_DISPLAY_POWER_HIGH	0x03

#define BU9795_IC_MSB_MASK	BIT(2)
#define BU9795_IC_MSB_0		0x00
#define BU9795_IC_MSB_1		0x04

#define BU9795_IC_RESET	0x02

#define BU9795_IC_CLOCK_MASK		BIT(0)
#define BU9795_IC_CLOCK_EXTERNAL	0x01
#define BU9795_IC_CLOCK_INTERMAL	0x00

#define BU9795_BLINK_RATE_MASK	(BIT(1) | BIT(0))
#define BU9795_BLINK_RATE_OFF	0x00
#define BU9795_BLINK_RATE_HALF	0x01
#define BU9795_BLINK_RATE_1		0x02
#define BU9795_BLINK_RATE_2		0x03

#define BU9795_ALL_PIXELS_MASK	(BIT(1) | BIT(0))
#define BU9795_ALL_PIXELS_ON	0x02
#define BU9795_ALL_PIXELS_OFF	0x01

struct bu9795_data {
	struct device *spi_dev;
	struct spi_config spi_config;
	struct spi_cs_control spi_cs;	
};

struct bu9795_config {
	const char *spi_dev_name;
	const char *spi_cs_dev_name;
	uint8_t spi_cs_pin;
	struct spi_config spi_cfg;
};

static void bu9795_set_mode(uint8_t display_on, uint8_t bias){
	uint8_t command = BU9795_CMD_MODE | (display_on & DISPLAY_MODE_BIT) | (bias & DISPLAY_BIAS_LEVEL_BIT);
}

static void bu9795_set_address(uint8_t address){
	uint8_t command = BU9795_CMD_ADDRESS | (address & BU9795_ADDRESS_MASK);
}

static void bu9795_set_display_control(uint8_t frequency, uint8_t waveform, uint8_t power_mode){
	uint8_t command = BU9795_CMD_DISPLAY_CONTROL 
		| (frequency & BU9795_DISPLAY_FREQ_MASK) 
		| (waveform & BU9795_DISPLAY_WAVEFORM_MASK) 
		| (power_mode & BU9795_DISPLAY_POWER_MASK);
}

static void bu9795_reset(){
	uint8_t command = BU9795_CMD_IC | BU9795_IC_RESET;
}

static void bu9795_set_ic(uint8_t msb, uint8_t clock_source){
	uint8_t command = BU9795_CMD_IC 
		| (msb & BU9795_IC_MSB_MASK)
		| (clock_source & BU9795_IC_CLOCK_MASK);
}

static void bu9795_set_blick_rate(uint8_t rate){
	uint8_t command = BU9795_CMD_BLINK
		| (rate & BU9795_BLINK_RATE_MASK); 
}

static void bu9795_set_all_pixels(uint8_t state){
	uint8_t command = BU9795_CMD_ALL_PIXEL
		| (state & BU9795_ALL_PIXELS_MASK);
}

static void print_impl(struct device *dev)
{
	printk("Hello World from the kernel\n");
}

static int bu9795_init(struct device *dev)
{
	struct bu9795_data *data = dev->driver_data;
	struct bu9795_config *config = dev->config->config_info;

	data->spi_dev = device_get_binding(config->spi_cs_dev_name);
	if (data->spi_dev == NULL) {
		LOG_ERR("SPI master device '%s' not found", config->spi_dev_name);
		return -EINVAL;
	}

	if (config->spi_cs_dev_name != NULL) {
		data->spi_cs.gpio_dev = device_get_binding(config->spi_cs_dev_name);
		if (data->spi_cs.gpio_dev == NULL) {
			LOG_ERR("SPI CS GPIO device '%s' not found", config->spi_cs_dev_name);
			return -EINVAL;
		}

		data->spi_cs.gpio_pin = config->spi_cs_pin;
	}

	return 0;
}

static const struct bu9795_driver_api bu9795_driver_api_impl = { 
	.print = print_impl,
};

// TODO: Somehow generate the following for each instance of BU97975
#if DT_INST_0_ROHM_BU9795

static struct bu9795_data bu9795_data_0 = {
};

static const struct bu9795_config bu9795_config_0 = {
	.spi_dev_name = DT_INST_0_ROHM_BU9795_BUS_NAME,
	.spi_cs_dev_name = DT_INST_0_ROHM_BU9795_CS_GPIOS_CONTROLLER,
	.spi_cs_pin = DT_INST_0_ROHM_BU9795_CS_GPIOS_PIN,
	.spi_cfg = {
		.operation = (SPI_OP_MODE_MASTER | SPI_TRANSFER_MSB | SPI_WORD_SET(8)),
		.frequency = DT_INST_0_ROHM_BU9795_SPI_MAX_FREQUENCY,
		.slave = DT_INST_0_ROHM_BU9795_BASE_ADDRESS,
		.cs = &bu9795_data_0.spi_cs,
	},
};


DEVICE_AND_API_INIT(bu9795_0, DT_INST_0_ROHM_BU9795_LABEL,
		    bu9795_init, &bu9795_data_0, &bu9795_config_0,
		    PRE_KERNEL_1, CONFIG_KERNEL_INIT_PRIORITY_DEVICE,
		    &bu9795_driver_api_impl);

#endif