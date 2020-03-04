/* main.c - Application main entry point */

#include <stdio.h>

#include <zephyr.h>
#include <device.h>
#include <drivers/gpio.h>
#include <sys/printk.h>
// TODO: Abstract this API to a generic segmented display
#include <bu9795_driver.h>


static struct gpio_callback button_cb_data;

struct device *dev_button = NULL;
struct device *dev_segment = NULL;

#if CONFIG_BU9795_TEST_PATTERN
int segment_pattern_stage = 0;
#endif

void button_pressed(struct device *dev, struct gpio_callback *cb, u32_t pins)
{
    printk("Button pressed at %" PRIu32 "\n", k_cycle_get_32());

#if CONFIG_BU9795_TEST_PATTERN
    // With each button press, increment the pattern stage to help identify the segment mapping
    if(dev == dev_button && (pins & BIT(DT_ALIAS_SW0_GPIOS_PIN)) && dev_segment != NULL)
    {
        bu9795_set_test_pattern(dev_segment, segment_pattern_stage++);
    }
#endif
}

void main(void)
{
    int ret;

    printk("Hello world!\n");

    dev_button = device_get_binding(DT_ALIAS_SW0_GPIOS_CONTROLLER);
    if (dev_button == NULL) {
        printk("Error: didn't find %s device\n", DT_ALIAS_SW0_GPIOS_CONTROLLER);
        return;
    }

    dev_segment = device_get_binding(DT_ALIAS_SEGMENT0_LABEL);
    if (dev_segment == NULL) {
        printk("Error: didn't find %s device\n", DT_ALIAS_SEGMENT0_LABEL);
        return;
    }
    bu9795_clear(dev_segment);

    ret = gpio_pin_configure(dev_button, DT_ALIAS_SW0_GPIOS_PIN, DT_ALIAS_SW0_GPIOS_FLAGS | GPIO_INPUT);
    if (ret != 0) {
        printk("Error %d: failed to configure pin %d '%s'\n", ret, DT_ALIAS_SW0_GPIOS_PIN, DT_ALIAS_SW0_LABEL);
        return;
    }

    ret = gpio_pin_interrupt_configure(dev_button, DT_ALIAS_SW0_GPIOS_PIN, GPIO_INT_EDGE_TO_ACTIVE);
    if (ret != 0) {
        printk("Error %d: failed to configure interrupt on pin %d '%s'\n", ret, DT_ALIAS_SW0_GPIOS_PIN, DT_ALIAS_SW0_LABEL);
        return;
    }

    gpio_init_callback(&button_cb_data, button_pressed, BIT(DT_ALIAS_SW0_GPIOS_PIN));
    gpio_add_callback(dev_button, &button_cb_data);

    printk("Press %s on the board\n", DT_ALIAS_SW0_LABEL);

    while (1) {
        k_sleep(1000);
    }
}
