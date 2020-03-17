#include <zephyr/types.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <sys/printk.h>
#include <sys/byteorder.h>
#include <zephyr.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/conn.h>
#include <bluetooth/uuid.h>
#include <bluetooth/gatt.h>
#include <bluetooth/services/bas.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(bluetooth_ess_service, LOG_LEVEL_INF);

// ESS error definitions
#define ESS_ERR_WRITE_REJECT    0x80
#define ESS_ERR_COND_NOT_SUPP   0x81

// ESS Trigger Setting conditions
#define ESS_TRIGGER_INACTIVE                0x00
#define ESS_FIXED_TIME_INTERVAL             0x01
#define ESS_NO_LESS_THAN_SPECIFIED_TIME     0x02
#define ESS_VALUE_CHANGED                   0x03
#define ESS_LESS_THAN_REF_VALUE             0x04
#define ESS_LESS_OR_EQUAL_TO_REF_VALUE      0x05
#define ESS_GREATER_THAN_REF_VALUE          0x06
#define ESS_GREATER_OR_EQUAL_TO_REF_VALUE   0x07
#define ESS_EQUAL_TO_REF_VALUE              0x08
#define ESS_NOT_EQUAL_TO_REF_VALUE          0x09

#define ESS_MEASUREMENT_FLAG_NOTIFY_MEASUREMENT BIT(1)

extern struct bt_conn *default_conn;

struct es_measurement {
    // Reserved for Future Use
    u16_t flags;
    u8_t sampling_func;
    u32_t meas_period;
    u32_t update_interval;
    u8_t application;
    u8_t meas_uncertainty;
};

struct ess_sensor {
    s16_t value;

    // Valid Range
    s16_t lower_limit;
    s16_t upper_limit;

    // ES trigger setting - Value Notification condition
    u8_t condition;
    u16_t ccc;
    union {
        u32_t seconds;
        // Reference temperature
        s16_t ref_val;
    };

    struct es_measurement meas;
};

#define SENSOR_TEMPERATURE_NAME "Temperature"

static struct ess_sensor sensor_temp = {
    .value = 1200,
    .lower_limit = 0,
    .upper_limit = 6500,
    .condition = ESS_VALUE_CHANGED,
    .meas.sampling_func = 0x01,
    .meas.meas_period = 0x00,
    .meas.update_interval = 1,
    .meas.application = 0x01,
    .meas.meas_uncertainty = 0x01,
};

#define SENSOR_Humidity_NAME "Humidity"

static struct ess_sensor sensor_humid = {
    .value = 5000,
    .lower_limit = 0,
    .upper_limit = 10000,
    .condition = ESS_VALUE_CHANGED,
    .meas.sampling_func = 0x01,
    .meas.meas_period = 0x00,
    .meas.update_interval = 1,
    .meas.application = 0x01,
    .meas.meas_uncertainty = 0x01,
};


static ssize_t read_u16(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf, u16_t len, u16_t offset)
{
	const u16_t *u16 = attr->user_data;
	u16_t value = sys_cpu_to_le16(*u16);

	return bt_gatt_attr_read(conn, attr, buf, len, offset, &value, sizeof(value));
}

static void humid_ccc_cfg_changed(const struct bt_gatt_attr *attr, u16_t value)
{
    sensor_humid.ccc = value;
}
static void temp_ccc_cfg_changed(const struct bt_gatt_attr *attr, u16_t value)
{
    sensor_temp.ccc = value;
}

struct read_es_measurement_rp {
    // Reserved for Future Use
    u16_t flags;
    u8_t sampling_function;
    u8_t measurement_period[3];
    u8_t update_interval[3];
    u8_t application;
    u8_t measurement_uncertainty;
} __packed;

static ssize_t read_es_measurement(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf, u16_t len, u16_t offset)
{
    const struct es_measurement *value = attr->user_data;
    struct read_es_measurement_rp rsp;

    rsp.flags = sys_cpu_to_le16(value->flags);
    rsp.sampling_function = value->sampling_func;
    sys_put_le24(value->meas_period, rsp.measurement_period);
    sys_put_le24(value->update_interval, rsp.update_interval);
    rsp.application = value->application;
    rsp.measurement_uncertainty = value->meas_uncertainty;

    return bt_gatt_attr_read(conn, attr, buf, len, offset, &rsp, sizeof(rsp));
}

static ssize_t read_ess_valid_range(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf, u16_t len, u16_t offset)
{
    const struct ess_sensor *sensor = attr->user_data;
    u16_t tmp[] = {
        sys_cpu_to_le16(sensor->lower_limit),
        sys_cpu_to_le16(sensor->upper_limit)
    };

    return bt_gatt_attr_read(conn, attr, buf, len, offset, tmp, sizeof(tmp));
}

struct es_trigger_setting_seconds {
    u8_t condition;
    u8_t sec[3];
} __packed;

struct es_trigger_setting_reference {
    u8_t condition;
    s16_t ref_val;
} __packed;

static ssize_t read_ess_trigger_setting(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf, u16_t len, u16_t offset)
{
    const struct ess_sensor *sensor = attr->user_data;
    struct es_trigger_setting_reference settings_reference;
    struct es_trigger_setting_seconds settings_seconds;

    switch (sensor->condition) {
        // Operand N/A
        case ESS_TRIGGER_INACTIVE: // fallthrough
        case ESS_VALUE_CHANGED:
            return bt_gatt_attr_read(conn, attr, buf, len, offset, &sensor->condition, sizeof(sensor->condition));
        // Seconds
        case ESS_FIXED_TIME_INTERVAL: // fallthrough
        case ESS_NO_LESS_THAN_SPECIFIED_TIME:

            settings_seconds.condition = sensor->condition;
            sys_put_le24(sensor->seconds, settings_seconds.sec);

            return bt_gatt_attr_read(conn, attr, buf, len, offset, &settings_seconds, sizeof(settings_seconds));

        // Reference temperature
        default:

            settings_reference.condition = sensor->condition;
            settings_reference.ref_val = sys_cpu_to_le16(sensor->ref_val);

            return bt_gatt_attr_read(conn, attr, buf, len, offset, &settings_reference, sizeof(settings_reference));
    }
}

static bool check_condition(u8_t condition, s16_t old_val, s16_t new_val, s16_t ref_val)
{
    switch (condition) {
        case ESS_TRIGGER_INACTIVE:
            return false;
        case ESS_FIXED_TIME_INTERVAL:
        case ESS_NO_LESS_THAN_SPECIFIED_TIME:
            // TODO: Check time requirements
            return false;
        case ESS_VALUE_CHANGED:
            return new_val != old_val;
        case ESS_LESS_THAN_REF_VALUE:
            return new_val < ref_val;
        case ESS_LESS_OR_EQUAL_TO_REF_VALUE:
            return new_val <= ref_val;
        case ESS_GREATER_THAN_REF_VALUE:
            return new_val > ref_val;
        case ESS_GREATER_OR_EQUAL_TO_REF_VALUE:
            return new_val >= ref_val;
        case ESS_EQUAL_TO_REF_VALUE:
            return new_val == ref_val;
        case ESS_NOT_EQUAL_TO_REF_VALUE:
            return new_val != ref_val;
        default:
            return false;
    }
}

static void update_ess_value(struct bt_conn *conn, const struct bt_gatt_attr *chrc, s16_t value, struct ess_sensor *sensor)
{
    if(sensor == &sensor_temp){
        LOG_DBG("Updating temperature");
    } else if (sensor == &sensor_humid){
        LOG_DBG("Updating humidity");
    } else {
        LOG_DBG("I don't know what i'm updateing 🤷‍♀️");
    }

    bool notify = check_condition(sensor->condition, sensor->value, value, sensor->ref_val);
    LOG_DBG("Condition: %02X, Ref: %d, Old Value: %d, New Value: %d => %s", sensor->condition, sensor->ref_val, sensor->value, value, notify ? "true" : "false");

    // Update temperature value
    sensor->value = value;

    // Trigger notification if conditions are met
    if (notify){
        if (sensor->ccc == BT_GATT_CCC_NOTIFY) {
            value = sys_cpu_to_le16(sensor->value);

            bt_gatt_notify(conn, chrc, &value, sizeof(value));
        }
    }
}

BT_GATT_SERVICE_DEFINE(ess,
    BT_GATT_PRIMARY_SERVICE(BT_UUID_ESS),

    // Temperature Sensor
    BT_GATT_CHARACTERISTIC(BT_UUID_TEMPERATURE,
                   BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                   BT_GATT_PERM_READ,
                   read_u16, NULL, &sensor_temp.value),
    BT_GATT_DESCRIPTOR(BT_UUID_ES_MEASUREMENT, BT_GATT_PERM_READ,
               read_es_measurement, NULL, &sensor_temp.meas),
    BT_GATT_CUD(SENSOR_TEMPERATURE_NAME, BT_GATT_PERM_READ),
    BT_GATT_DESCRIPTOR(BT_UUID_VALID_RANGE, BT_GATT_PERM_READ,
               read_ess_valid_range, NULL, &sensor_temp),
    BT_GATT_DESCRIPTOR(BT_UUID_ES_TRIGGER_SETTING,
               BT_GATT_PERM_READ, read_ess_trigger_setting,
               NULL, &sensor_temp),
    BT_GATT_CCC(temp_ccc_cfg_changed,
            BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),

    // Humidity Sensor
	BT_GATT_CHARACTERISTIC(BT_UUID_HUMIDITY,
			       BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
			       BT_GATT_PERM_READ,
			       read_u16, NULL, &sensor_humid.value),
	BT_GATT_DESCRIPTOR(BT_UUID_ES_MEASUREMENT, BT_GATT_PERM_READ,
			   read_es_measurement, NULL, &sensor_humid.meas),
	BT_GATT_CUD(SENSOR_Humidity_NAME, BT_GATT_PERM_READ),
	BT_GATT_DESCRIPTOR(BT_UUID_VALID_RANGE, BT_GATT_PERM_READ,
			   read_ess_valid_range, NULL, &sensor_humid),
	BT_GATT_DESCRIPTOR(BT_UUID_ES_TRIGGER_SETTING,
			   BT_GATT_PERM_READ, read_ess_trigger_setting,
			   NULL, &sensor_humid),
	BT_GATT_CCC(humid_ccc_cfg_changed,
		    BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
);

void bluetooth_update_temperature(u16_t value)
{
    update_ess_value(default_conn, &ess.attrs[2], value, &sensor_temp);
}

void bluetooth_update_humidity(u16_t value)
{
    update_ess_value(default_conn, &ess.attrs[8], value, &sensor_humid);
}
