#include <zephyr.h>
#include <init.h>
#include <logging/log.h>
LOG_MODULE_REGISTER(display, LOG_LEVEL_DBG);

// TODO: Abstract this API to a generic segmented display
#include <bu9795_driver.h>

#include "display.h"


static struct device *dev_segment = NULL;
static u32_t set_symbols = 0;

int display_set_temperature(const struct sensor_value *value)
{
    if (dev_segment == NULL) {
        return -ENOENT;
    }

    if (value == NULL) {
        // TODO: disable decimal point
        bu9795_set_segment(dev_segment, 0, -1);
        bu9795_set_segment(dev_segment, 1, -1);
        bu9795_set_segment(dev_segment, 2, -1);
    } else {
        bu9795_set_segment(dev_segment, 0, value->val1 / 10);
        bu9795_set_segment(dev_segment, 1, value->val1 % 10);
        bu9795_set_segment(dev_segment, 2, value->val2 / 100000);
    }

    bu9795_flush(dev_segment);
    return 0;
}

int display_set_humidity(const struct sensor_value *value)
{
    if (dev_segment == NULL) {
        return -ENOENT;
    }

    if (value == NULL) {
        // TODO: disable decimal point
        bu9795_set_segment(dev_segment, 3, -1);
        bu9795_set_segment(dev_segment, 4, -1);
        bu9795_set_segment(dev_segment, 5, -1);
    } else {
        bu9795_set_segment(dev_segment, 3, value->val1 / 10);
        bu9795_set_segment(dev_segment, 4, value->val1 % 10);
        bu9795_set_segment(dev_segment, 5, value->val2 / 100000);
    }

    bu9795_flush(dev_segment);
    return 0;
}

int display_set_battery(int percent)
{
    if (dev_segment == NULL) {
        return -ENOENT;
    }

    if (percent > 80) {
        bu9795_set_segment(dev_segment, 0, 6);
    } else if (percent > 60) {
        bu9795_set_segment(dev_segment, 0, 5);
    } else if (percent > 40) {
        bu9795_set_segment(dev_segment, 0, 4);
    } else if (percent > 20) {
        bu9795_set_segment(dev_segment, 0, 3);
    } else if (percent > 0) {
        bu9795_set_segment(dev_segment, 0, 2);
    } else {
        bu9795_set_segment(dev_segment, 0, 1);
    }

    bu9795_flush(dev_segment);
    return 0;
}

int display_set_symbols(u8_t symbols)
{
    set_symbols |= symbols;
    bu9795_set_symbol(dev_segment, set_symbols);

    bu9795_flush(dev_segment);
    return 0;
}

int display_clear_symbols(u8_t symbols)
{
    set_symbols &= ~symbols;
    bu9795_set_symbol(dev_segment, set_symbols);

    bu9795_flush(dev_segment);
    return 0;
}

static int display_setup(struct device *arg)
{
	dev_segment = device_get_binding(DT_ALIAS_SEGMENT0_LABEL);
    if (dev_segment == NULL) {
        LOG_ERR("Didn't find %s device", DT_ALIAS_SEGMENT0_LABEL);
        return -ENOENT;
    }

    display_set_temperature(NULL);

    display_clear_symbols(DISPLAY_SYMBOL_ALL);

    // Default the battery logo to empty
    display_set_battery(0);

    return 0;
}

SYS_INIT(display_setup, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
