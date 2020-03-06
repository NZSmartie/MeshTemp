/* main.c - Application main entry point */

#include <stdio.h>

#include <zephyr.h>
#include <device.h>
#include <drivers/gpio.h>
#include <sys/printk.h>
// TODO: Abstract this API to a generic segmented display
#include <bu9795_driver.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(Application, LOG_LEVEL_DBG);

static struct gpio_callback button_cb_data;

struct device *dev_button = NULL;
struct device *dev_segment = NULL;

#if CONFIG_BU9795_TEST_PATTERN
int segment_pattern_stage = 0;
#else
int segment_0_value = 0;
#endif

void button_pressed(struct device *dev, struct gpio_callback *cb, u32_t pins)
{
    LOG_INF("Button pressed at %" PRIu32 "\n", k_cycle_get_32());

#if CONFIG_BU9795_TEST_PATTERN
    // With each button press, increment the pattern stage to help identify the segment mapping
    if(dev == dev_button && (pins & BIT(DT_ALIAS_SW0_GPIOS_PIN)) && dev_segment != NULL)
    {
        bu9795_set_test_pattern(dev_segment, segment_pattern_stage++);
    }
#else
    bu9795_set_segment(dev_segment, 0, segment_0_value++ % 10);
#endif
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

    dev_segment = device_get_binding(DT_ALIAS_SEGMENT0_LABEL);
    if (dev_segment == NULL) {
        LOG_ERR("Didn't find %s device", DT_ALIAS_SEGMENT0_LABEL);
        return;
    }
    // bu9795_clear(dev_segment);

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

    while (1) {
        k_sleep(1000);
    }
}
