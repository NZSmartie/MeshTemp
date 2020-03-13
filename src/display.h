#pragma once

#include <zephyr/types.h>
#include <drivers/sensor.h>

enum display_symbols {
    DISPLAY_SYMBOL_TEMPERATURE_DECIMAL = 0x01,
    DISPLAY_SYMBOL_BLUETOOTH = 0x02,
    DISPLAY_SYMBOL_CELSIUS = 0x04,
    DISPLAY_SYMBOL_HORIZONTAL_RULE = 0x08,
    DISPLAY_SYMBOL_HUMIDITY_DECIMAL = 0x10,
    DISPLAY_SYMBOL_HUMIDITY = 0x20,
    DISPLAY_SYMBOL_ALL = 0xFF,

};

int display_set_temperature(const struct sensor_value *value);
int display_set_humidity(const struct sensor_value *value);
int display_set_battery(int percent);
int display_set_symbols(u8_t symbols);
int display_clear_symbols(u8_t symbols);
