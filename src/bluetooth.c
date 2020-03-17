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
LOG_MODULE_REGISTER(bluetooth, LOG_LEVEL_INF);

/* ESS error definitions */
#define ESS_ERR_WRITE_REJECT    0x80
#define ESS_ERR_COND_NOT_SUPP   0x81

/* ESS Trigger Setting conditions */
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

#if CONFIG_BT_SHELL
extern
#endif
struct bt_conn *default_conn;

static const struct bt_data bluettoth_advertise_data[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_UUID16_ALL,
		      0x1a, 0x18, /* Environmental Sensing Service */
		      0x0a, 0x18, /* Device Information Service */
		      0x0f, 0x18), /* Battery Service */
};

static void bluetooth_connected(struct bt_conn *conn, u8_t err)
{
	if (err) {
		LOG_WRN("Connection failed (err 0x%02x)", err);
	} else {
		default_conn = bt_conn_ref(conn);
		LOG_INF("Bluetooth connected");
	}
}

static void bluetooth_disconnected(struct bt_conn *conn, u8_t reason)
{
	LOG_INF("Disconnected (reason 0x%02x)", reason);

	if (default_conn) {
		bt_conn_unref(default_conn);
		default_conn = NULL;
	}
}

static struct bt_conn_cb bluetooth_connection_callbacks = {
	.connected = bluetooth_connected,
	.disconnected = bluetooth_disconnected,
};

static void auth_confirm(struct bt_conn *conn)
{

}

static void auth_cancel(struct bt_conn *conn)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	LOG_WRN("Pairing cancelled: %s", addr);
}

static struct bt_conn_auth_cb bluetooth_auth_cb_display = {
	.cancel = auth_cancel,
    .pairing_confirm = auth_confirm,
};

void bluetooth_ready()
{
	int ret;

	ret = bt_le_adv_start(BT_LE_ADV_CONN_NAME, bluettoth_advertise_data, ARRAY_SIZE(bluettoth_advertise_data), NULL, 0);
	if (ret < 0) {
		LOG_ERR("Advertising failed to start (%d)", ret);
		return;
	}

	LOG_DBG("Advertising successfully started");

    bt_conn_cb_register(&bluetooth_connection_callbacks);
	bt_conn_auth_cb_register(&bluetooth_auth_cb_display);

	LOG_DBG("Initialized");
}

void bluetooth_update_battery(u8_t level)
{
#if CONFIG_BT_GATT_BAS
    bt_gatt_bas_set_battery_level(level);
#endif
}
