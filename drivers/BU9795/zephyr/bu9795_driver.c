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
LOG_MODULE_REGISTER(bu9795, CONFIG_BU9795_LOG_LEVEL);

#define BU9795_SEG_REGISTER_SIZE 15

#define BU9795_CMD_DATA_BIT BIT(7)

#define BU9795_CMD_ADDRESS          0x00
#define BU9795_CMD_MODE             0x40
#define BU9795_CMD_DISPLAY_CONTROL  0x20
#define BU9795_CMD_IC               0x68
#define BU9795_CMD_BLINK            0x70
#define BU9795_CMD_ALL_PIXEL        0x7C

#define DISPLAY_MODE_BIT    BIT(3)
#define DISPLAY_ON          0x04
#define DISPLAY_OFF         0x00

#define DISPLAY_BIAS_LEVEL_BIT  BIT(2)
#define DISPLAY_BIAS_LEVEL_1_3  0x00
#define DISPLAY_BIAS_LEVEL_1_2  0x02

#define BU9795_ADDRESS_MASK 0x1F

#define BU9795_DISPLAY_FREQ_MASK    (BIT(4) | BIT(3))
#define BU9795_DISPLAY_FREQ_80      0x00
#define BU9795_DISPLAY_FREQ_71      0x04
#define BU9795_DISPLAY_FREQ_64      0x08
#define BU9795_DISPLAY_FREQ_53      0x0C

#define BU9795_DISPLAY_WAVEFORM_MASK    BIT(2)
#define BU9795_DISPLAY_WAVEFORM_LINE    0x00
#define BU9795_DISPLAY_WAVEFORM_FRAME   0x04

#define BU9795_DISPLAY_POWER_MASK   (BIT(1) | BIT(0))
#define BU9795_DISPLAY_POWER_SAVE_1 0x00
#define BU9795_DISPLAY_POWER_SAVE_2 0x01
#define BU9795_DISPLAY_POWER_NORMAL 0x02
#define BU9795_DISPLAY_POWER_HIGH   0x03

#define BU9795_IC_MSB_MASK  BIT(2)
#define BU9795_IC_MSB_0     0x00
#define BU9795_IC_MSB_1     0x04

#define BU9795_IC_RESET 0x02

#define BU9795_IC_CLOCK_MASK        BIT(0)
#define BU9795_IC_CLOCK_EXTERNAL    0x01
#define BU9795_IC_CLOCK_INTERMAL    0x00

#define BU9795_BLINK_RATE_MASK  (BIT(1) | BIT(0))
#define BU9795_BLINK_RATE_OFF   0x00
#define BU9795_BLINK_RATE_HALF  0x01
#define BU9795_BLINK_RATE_1     0x02
#define BU9795_BLINK_RATE_2     0x03

#define BU9795_ALL_PIXELS_MASK  (BIT(1) | BIT(0))
#define BU9795_ALL_PIXELS_ON    0x02
#define BU9795_ALL_PIXELS_OFF   0x01

struct bu9795_data {
    struct device *spi_dev;
    struct spi_config spi_config;
    struct spi_cs_control spi_cs;

    // Local copy of seg register
    u8_t data[BU9795_SEG_REGISTER_SIZE];
};

struct bu9795_config {
    const char *spi_dev_name;
    const char *spi_cs_dev_name;
    uint8_t spi_cs_pin;
    struct spi_config spi_cfg;
};

static uint8_t bu9795_set_mode(uint8_t display_on, uint8_t bias){
    return BU9795_CMD_MODE | (display_on & DISPLAY_MODE_BIT) | (bias & DISPLAY_BIAS_LEVEL_BIT);
}

static uint8_t bu9795_set_address(uint8_t address){
    return BU9795_CMD_ADDRESS | (address & BU9795_ADDRESS_MASK);
}

static uint8_t bu9795_set_display_control(uint8_t frequency, uint8_t waveform, uint8_t power_mode){
    return BU9795_CMD_DISPLAY_CONTROL
        | (frequency & BU9795_DISPLAY_FREQ_MASK)
        | (waveform & BU9795_DISPLAY_WAVEFORM_MASK)
        | (power_mode & BU9795_DISPLAY_POWER_MASK);
}

static uint8_t bu9795_reset(){
    return BU9795_CMD_IC | BU9795_IC_RESET;
}

static uint8_t bu9795_set_ic(uint8_t msb, uint8_t clock_source){
    return BU9795_CMD_IC
        | (msb & BU9795_IC_MSB_MASK)
        | (clock_source & BU9795_IC_CLOCK_MASK);
}

static uint8_t bu9795_set_blick_rate(uint8_t rate){
    return BU9795_CMD_BLINK
        | (rate & BU9795_BLINK_RATE_MASK);
}

static uint8_t bu9795_set_all_pixels(uint8_t state){
    return BU9795_CMD_ALL_PIXEL
        | (state & BU9795_ALL_PIXELS_MASK);
}

static int bu9795_write_cmd(struct device *dev, uint8_t cmd) {
    struct bu9795_data *data = dev->driver_data;
    const struct bu9795_config *config = dev->config->config_info;
    struct spi_buf tx_buf[1];
    struct spi_buf_set tx;
    int err;

    tx_buf[0].buf = &cmd;
    tx_buf[0].len = 1;

    tx.buffers = tx_buf;
    tx.count = 1;

    err = spi_write(data->spi_dev, &config->spi_cfg, &tx);

    return err;
}

static int bu9795_write_data(struct device *dev, u8_t addr, const u8_t *payload, u8_t len)
{
    struct bu9795_data *data = dev->driver_data;
    const struct bu9795_config *config = dev->config->config_info;
    u8_t command = bu9795_set_address(addr) | BU9795_CMD_DATA_BIT;
    struct spi_buf tx_buf[] = {
        { .buf = &command, .len = 1},
        { .buf = (void*)payload, .len = len},
    };
    struct spi_buf_set tx = {
        .buffers = tx_buf,
        .count = 2,
    };

    return spi_write(data->spi_dev, &config->spi_cfg, &tx);
}

static int bu9795_init(struct device *dev)
{
    struct bu9795_data *data = dev->driver_data;
    const struct bu9795_config *config = dev->config->config_info;
    int err;
    uint8_t command;

    data->spi_dev = device_get_binding(config->spi_cs_dev_name);
    if (data->spi_dev == NULL) {
        LOG_ERR("SPI master device '%s' not found", config->spi_dev_name);
        return -EINVAL;
    } else {
        LOG_DBG("SPI master device '%s' found", config->spi_dev_name);
    }

    if (config->spi_cs_dev_name != NULL) {
        data->spi_cs.gpio_dev = device_get_binding(config->spi_cs_dev_name);
        if (data->spi_cs.gpio_dev == NULL) {
            LOG_ERR("SPI CS GPIO device '%s' not found", config->spi_cs_dev_name);
            return -EINVAL;
        } else {
            LOG_DBG("SPI CS GPIO device '%s' found", config->spi_cs_dev_name);
        }

        data->spi_cs.gpio_pin = config->spi_cs_pin;
    } else {
        LOG_WRN("SPI CS GPIO device not configured");
    }

    // Reset the segment display driver
    command = bu9795_reset();
    err = bu9795_write_cmd(dev, command);
    if(err){
        LOG_ERR("Failed to reset BU9795: SPI error '%d'", err);
        return err;
    }


    command = bu9795_set_blick_rate(BU9795_BLINK_RATE_HALF);
    err = bu9795_write_cmd(dev, command);
    if(err){
        LOG_ERR("Failed to set blink rate on BU9795: SPI error '%d'", err);
        return err;
    }

    command = bu9795_set_display_control(BU9795_DISPLAY_FREQ_80, BU9795_DISPLAY_WAVEFORM_FRAME, BU9795_DISPLAY_POWER_NORMAL);
    err = bu9795_write_cmd(dev, command);
    if(err){
        LOG_ERR("Failed to set display on BU9795: SPI error '%d'", err);
        return err;
    }

    command = bu9795_set_ic(BU9795_IC_MSB_0, BU9795_IC_CLOCK_INTERMAL);
    err = bu9795_write_cmd(dev, command);
    if(err){
        LOG_ERR("Failed to configure BU9795: SPI error '%d'", err);
        return err;
    }

    command = bu9795_set_address(0);
    err = bu9795_write_cmd(dev, command);
    if(err){
        LOG_ERR("Failed to configure BU9795: SPI error '%d'", err);
        return err;
    }

    err = bu9795_write_data(dev, 0, data->data, 15);
    if(err){
        LOG_ERR("Failed initialise segment data on BU9795: SPI error '%d'", err);
        return err;
    }

    command = bu9795_set_mode(DISPLAY_ON, DISPLAY_BIAS_LEVEL_1_3);
    err = bu9795_write_cmd(dev, command);
    if(err){
        LOG_ERR("Failed to set display mode on BU9795: SPI error '%d'", err);
        return err;
    }

    return 0;
}

static void clear_impl(struct device *dev)
{
    struct bu9795_data *data = dev->driver_data;

    // Clear the local copy of the segment register first
    memset(data->data, 0, BU9795_SEG_REGISTER_SIZE);
    bu9795_write_data(dev, 0, data->data, BU9795_SEG_REGISTER_SIZE);
}

static void set_segment_impl(struct device *dev, u8_t segment, u8_t value)
{
    // TODO: setup a mapping (or lookup table) fromBU9795's seg0...30 to the desired segment and value.
}

static void set_symbol_impl(struct device *dev, u8_t symbol, u8_t state)
{
    // TODO: setup a mapping (or lookup table) fromBU9795's seg0...30 to the desired symbol and state.
}

#if CONFIG_BU9795_TEST_PATTERN
static void set_test_pattern_impl(struct device *dev, u8_t stage)
{

}
#endif

static const struct bu9795_driver_api bu9795_driver_api_impl = {
    .clear = &clear_impl,
    .set_segment = &set_segment_impl,
    .set_symbol = &set_symbol_impl,
#if CONFIG_BU9795_TEST_PATTERN
    .set_test_pattern = &set_test_pattern_impl,
#endif
};

// TODO: Somehow generate the following for each instance of BU97975
#if DT_INST_0_ROHM_BU9795

static struct bu9795_data bu9795_data_0 = {
    .data = {0},
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
