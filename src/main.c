/* main.c - Application main entry point */

#include <stdio.h>

#include <zephyr.h>
#include <device.h>
#include <drivers/gpio.h>
#include <drivers/sensor.h>
#include <sys/printk.h>
// TODO: Abstract this API to a generic segmented display
#include <bu9795_driver.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

#include "battery.h"
#include "sensor.h"

static struct gpio_callback button_cb_data;

struct device *dev_button = NULL;
struct device *dev_segment = NULL;
struct device *dev_battery_voltage = NULL;

void button_pressed(struct device *dev, struct gpio_callback *cb, u32_t pins)
{
    LOG_INF("Button pressed at %" PRIu32 "\n", k_cycle_get_32());
}

void main(void)
{
    int ret;

    LOG_INF("Hello world!");

    dev_button = device_get_binding(DT_ALIAS_SW0_GPIOS_CONTROLLER);
    if (dev_button == NULL) {
        LOG_ERR("Didn't find %s device", DT_ALIAS_SW0_GPIOS_CONTROLLER);
        return;
    }

    // dev_battery_voltage = device_get_binding(DT_)

    dev_segment = device_get_binding(DT_ALIAS_SEGMENT0_LABEL);
    if (dev_segment == NULL) {
        LOG_ERR("Didn't find %s device", DT_ALIAS_SEGMENT0_LABEL);
        return;
    }

    ret = gpio_pin_configure(dev_button, DT_ALIAS_SW0_GPIOS_PIN, DT_ALIAS_SW0_GPIOS_FLAGS | GPIO_INPUT);
    if (ret != 0) {
        LOG_ERR("Failed to configure pin %d '%s' (Error %d)", DT_ALIAS_SW0_GPIOS_PIN, DT_ALIAS_SW0_LABEL, ret);
        return;
    }

    ret = gpio_pin_interrupt_configure(dev_button, DT_ALIAS_SW0_GPIOS_PIN, GPIO_INT_EDGE_TO_ACTIVE);
    if (ret != 0) {
        LOG_ERR("Failed to configure interrupt on pin %d '%s' (Error %d)", DT_ALIAS_SW0_GPIOS_PIN, DT_ALIAS_SW0_LABEL, ret);
        return;
    }

    gpio_init_callback(&button_cb_data, button_pressed, BIT(DT_ALIAS_SW0_GPIOS_PIN));
    gpio_add_callback(dev_button, &button_cb_data);

    LOG_INF("Press %s on the board", DT_ALIAS_SW0_LABEL);

    // Default the battery logo to empty
    bu9795_set_symbol(dev_segment, 0, 1);
    // Turn on default display symbols
    bu9795_set_symbol(dev_segment, 1, 3);

    struct sensor_value temp, hum;

    while(1)
    {
        int batt_mV = battery_sample();

		if (batt_mV >= 0)
        {
            int batt_pptt = battery_level_pptt(batt_mV, alkaline_level_point) / 100;
            LOG_INF("Battery: %d%% (%d.%03dV)", batt_pptt, batt_mV / 1000, batt_mV % 1000);

            if (batt_pptt > 80) {
                bu9795_set_symbol(dev_segment, 0, 6);
            } else if (batt_pptt > 60) {
                bu9795_set_symbol(dev_segment, 0, 5);
            } else if (batt_pptt > 40) {
                bu9795_set_symbol(dev_segment, 0, 4);
            } else if (batt_pptt > 20) {
                bu9795_set_symbol(dev_segment, 0, 3);
            } else if (batt_pptt > 0) {
                bu9795_set_symbol(dev_segment, 0, 2);
            } else {
                bu9795_set_symbol(dev_segment, 0, 1);
            }
        }
        else
        {
			LOG_ERR("Failed to read battery voltage: %d", batt_mV);
        }

        if (update_sensor(&temp, &hum) == 0)
        {
            if(dev_segment != NULL)
            {
                bu9795_set_segment(dev_segment, 0, temp.val1 / 10);
                bu9795_set_segment(dev_segment, 1, temp.val1 % 10);
                bu9795_set_segment(dev_segment, 2, temp.val2 / 100000);

                bu9795_set_segment(dev_segment, 3, hum.val1 / 10);
                bu9795_set_segment(dev_segment, 4, hum.val1 % 10);
                bu9795_set_segment(dev_segment, 5, hum.val2 / 100000);

                bu9795_flush(dev_segment);
            }
            LOG_INF("SHT3XD: %d.%d Cel ; %d.%d %%RH\n",
                temp.val1, temp.val2 / 100000,
                hum.val1, hum.val2 / 100000);
        }

        k_sleep(1000);
    }
}
