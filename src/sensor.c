#include <zephyr.h>
#include <device.h>
#include <init.h>
#include <drivers/sensor.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(sensor, LOG_LEVEL_INF);

struct device *dev_sensor = NULL;

int update_sensor(struct sensor_value *temp, struct sensor_value *hum)
{
    if(dev_sensor == NULL)
    {
        return -ENOENT;
    }

    LOG_DBG("Fetching sensor data");
    int ret = sensor_sample_fetch(dev_sensor);
    if (ret)
    {
        LOG_ERR("Could not get fetch sample from %s, errno: %d", dev_sensor->config->name, ret);
        return ret;
    }

    LOG_DBG("Getting SENSOR_CHAN_AMBIENT_TEMP");
    ret = sensor_channel_get(dev_sensor, SENSOR_CHAN_AMBIENT_TEMP, temp);
    if (ret)
    {
        LOG_ERR("Could not get SENSOR_CHAN_AMBIENT_TEMP from %s, errno: %d", dev_sensor->config->name, ret);
        return ret;
    }

    LOG_DBG("Getting SENSOR_CHAN_HUMIDITY");
    ret = sensor_channel_get(dev_sensor, SENSOR_CHAN_HUMIDITY, hum);
    if(ret)
    {
        LOG_ERR("Could not get SENSOR_CHAN_HUMIDITY from %s, errno: %d", dev_sensor->config->name, ret);
        return ret;
    }

    LOG_DBG("Sensor updated");

    return 0;
}

static int sensor_init(struct device *dev)
{
    ARG_UNUSED(dev);

    dev_sensor = device_get_binding(DT_ALIAS_SENSOR0_LABEL);
    if (dev_sensor == NULL) {
        LOG_ERR("Didn't find %s device", DT_ALIAS_SENSOR0_LABEL);
    }

    return 0;
}

SYS_INIT(sensor_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
