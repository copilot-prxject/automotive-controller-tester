// #include "modules/ble.h"

// #include <esp_log.h>

// #include <nimble/nimble_port.h>
// #include <nimble/nimble_port_freertos.h>
// #include <ble_svc_ans.h>
// #include <host/util/util.h>

// #include <services/gap/ble_svc_gap.h>
// #include <services/gatt/ble_svc_gatt.h>

// #include <ble_store_config.c>

// #include "nvs_flash.h"
#include "modules/ble.h"

#define CONFIG_LOG_DEFAULT_LEVEL DEBUG
#include <esp_log.h>
#include <host/util/util.h>
#include <nimble/nimble_port.h>
#include <nimble/nimble_port_freertos.h>
#include <services/ans/ble_svc_ans.h>
#include <services/gap/ble_svc_gap.h>
#include <services/gatt/ble_svc_gatt.h>

#include <../src/ble_store_config.c>

#include "nvs_flash.h"

#define GATT_SVR_SVC_ALERT_UUID 0x1811
#define GATT_MY_UUID 0x5000
#define GATT_CURRENT_MEASURE_CTRL 0x5001
#define GATT_CURRENT_MEASURE 0x5002
#define GATT_VOLTAE_MEASURE_CTRL 0x5003
#define GATT_VOLTAE_MEASURE 0x5004
#define GATT_PWM 0x5005
#define GATT_RELAY 0x5006
#define GATT_TEMPERATURE_CTRL 0x5007
#define GATT_TEMPERATURE 0x5008

static uint8_t own_addr_type = 0;
static uint16_t conn_handle;
static int on_ble_gap_event(struct ble_gap_event *event, void *arg);

static struct {
    CharacteristicCallback callback_pwm;
    CharacteristicCallback callback_relay;
    struct {
        Characteristic name;
        char value[11 + 11 + 11 + 11];
        uint16_t handle;
        CharacteristicCallback callback;
    } notify_chr[kLastMeasurementChr];
} ctx = {
    .callback_pwm = NULL,
    .callback_relay = NULL,
    .notify_chr = {
        { .name = kVoltage,     .handle = 0, .callback = NULL},
        { .name = kCurrent,     .handle = 0, .callback = NULL},
        { .name = kTemperature, .handle = 0, .callback = NULL},
    },
};

static int voltage_ctrl_callback(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg) {
    ESP_LOGI(__func__, "Voltage callback");

    ESP_LOGI(__func__, "Operation type: %d", ctxt->op);
    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        char parameters[10 + 1] = { 0 };
        struct os_mbuf *om;
        om = ctxt->om;
        uint8_t len = os_mbuf_len(om);
        len = len < sizeof(parameters) ? len : sizeof(parameters);

        assert(os_mbuf_copydata(om, 0, len, parameters) == 0);

        // parameters[len] = '\0';
        ESP_LOG_BUFFER_HEX("Incomming bytes:", parameters, len);
        ESP_LOGI(__func__, "Value: %s", parameters);
        ctx.notify_chr[kVoltage].callback(parameters, len);
        return 0;
    }

    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        int rc = os_mbuf_append(ctxt->om, &ctx.notify_chr[kVoltage].value, sizeof(ctx.notify_chr[kVoltage].value));
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }

    return 0;
}

static int current_ctrl_callback(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg) {
    ESP_LOGI(__func__, "Current callback");

    ESP_LOGI(__func__, "Operation type: %d", ctxt->op);
    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        char parameters[10 + 1] = { 0 };
        struct os_mbuf *om;
        om = ctxt->om;
        uint8_t len = os_mbuf_len(om);
        len = len < sizeof(parameters) ? len : sizeof(parameters);
        assert(os_mbuf_copydata(om, 0, len, parameters) == 0);
        // parameters[len] = '\0';
        ESP_LOG_BUFFER_HEX("Bytes:", parameters, len);
        ESP_LOGI(__func__, "Value: %s", parameters);

        // ctx.notify_chr[kCurrent].callback(parameters, len);
        return 0;
    }

    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        int rc = os_mbuf_append(ctxt->om, &ctx.notify_chr[kVoltage].value, sizeof(ctx.notify_chr[kVoltage].value));
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }

    return 0;
    int rc = os_mbuf_append(ctxt->om, &ctx.notify_chr[kCurrent].value, sizeof(ctx.notify_chr[kCurrent].value));
    return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

static int temperature_ctrl_callback(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg) {
    ESP_LOGI(__func__, "Temperature callback");

    ESP_LOGI(__func__, "Operation type: %d", ctxt->op);
    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        char parameters[10 + 1] = { 0 };
        struct os_mbuf *om;
        om = ctxt->om;
        uint8_t len = os_mbuf_len(om);
        len = len < sizeof(parameters) ? len : sizeof(parameters);

        assert(os_mbuf_copydata(om, 0, len, parameters) == 0);

        // parameters[len] = '\0';
        ESP_LOG_BUFFER_HEX("Incomming bytes:", parameters, len);
        ESP_LOGI(__func__, "Value: %s", parameters);
        ctx.notify_chr[kTemperature].callback(parameters, len);
        return 0;
    }

    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        int rc = os_mbuf_append(ctxt->om, &ctx.notify_chr[kTemperature].value, sizeof(ctx.notify_chr[kTemperature].value));
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }

    return 0;
}

static int pwm_ctrl_callback(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg) {
    ESP_LOGI(__func__, "PWM control callback");

    ESP_LOGI(__func__, "Operation type: %d", ctxt->op);
    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        char parameters[10 + 3 + 10 + 3 + 1] = { 0 };
        struct os_mbuf *om;
        om = ctxt->om;
        uint8_t len = os_mbuf_len(om);
        len = len < sizeof(parameters) ? len : sizeof(parameters);
        assert(os_mbuf_copydata(om, 0, len, parameters) == 0);
        // parameters[len] = '\0';
        ESP_LOG_BUFFER_HEX("Bytes:", parameters, len);
        ESP_LOGI(__func__, "Value: %s", parameters);
        ctx.callback_pwm(parameters, len);
        return 0;
    }

    return 0;
}

static int relay_ctrl_callback(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg) {
    ESP_LOGI(__func__, "RELAY control callback");

    ESP_LOGI(__func__, "Operation type: %d", ctxt->op);
    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        uint8_t parameters[10 + 1] = { 0 };
        struct os_mbuf *om;
        om = ctxt->om;
        uint8_t len = os_mbuf_len(om);
        len = len < sizeof(parameters) ? len : sizeof(parameters);
        assert(os_mbuf_copydata(om, 0, len, parameters) == 0);
        // parameters[len] = '\0';
        ESP_LOG_BUFFER_HEX("Bytes:", parameters, len);
        ESP_LOGI(__func__, "Value: %s", parameters);
        // parse_relay_parameters(parametes);
        return 0;
    }

    return 0;
}

static bool start_advertisement(void) {
    uint8_t own_addr_type;
    struct ble_gap_adv_params advertisement_parameters;
    struct ble_hs_adv_fields advertisement_fields;

    int rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        MODLOG_DFLT(ERROR, "error determining address type; rc=%d\n", rc);
        return false;
    }

    memset(&advertisement_fields, 0, sizeof(advertisement_fields));
    advertisement_fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    advertisement_fields.tx_pwr_lvl_is_present = 1;
    advertisement_fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;

    const char *device_name = ble_svc_gap_device_name();
    advertisement_fields.name = (uint8_t *)device_name;
    advertisement_fields.name_len = strlen(device_name);
    advertisement_fields.name_is_complete = 0;

    // advertisement_fields.uuids16 = (ble_uuid16_t[]){
    //     BLE_UUID16_INIT(GATT_SVR_SVC_ALERT_UUID)};
    advertisement_fields.num_uuids16 = 1;
    advertisement_fields.uuids16_is_complete = 1;

    rc = ble_gap_adv_set_fields(&advertisement_fields);
    if (rc != 0) {
        MODLOG_DFLT(ERROR, "error setting advertisement data; rc=%d\n", rc);
        return false;
    }

    memset(&advertisement_parameters, 0, sizeof(advertisement_parameters));

    advertisement_parameters.conn_mode = BLE_GAP_CONN_MODE_UND;
    advertisement_parameters.disc_mode = BLE_GAP_DISC_MODE_GEN;

    rc = ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER, &advertisement_parameters, on_ble_gap_event, NULL);
    if (rc != 0) {
        MODLOG_DFLT(ERROR, "error enabling advertisement; rc=%d\n", rc);
        return false;
    }

    ESP_LOGI("BLE", "Advertisement started");
    return true;
}

static int on_ble_gap_event(struct ble_gap_event *event, void *arg) {
    struct ble_gap_conn_desc desc;

    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            ESP_LOGI("BLE GAP Event", "Connection %s; status = %d ",
                     event->connect.status == 0 ? "established" : "failed",
                     event->connect.status);

            if (event->connect.status == 0) {
                int rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
                assert(rc == 0);
            }

            if (event->connect.status != 0)
                start_advertisement();

            conn_handle = event->connect.conn_handle;
            break;

        case BLE_GAP_EVENT_DISCONNECT:
            MODLOG_DFLT(INFO, "disconnect; reason = %d ", event->disconnect.reason);
            start_advertisement();
            break;

        case BLE_GAP_EVENT_SUBSCRIBE:
            MODLOG_DFLT(DEBUG,
                        "subscribe event; cur_notify=%d\n value handle; "
                        "val_handle=%d\n",
                        event->subscribe.cur_notify, event->subscribe.attr_handle);

            ESP_LOGD("BLE_GAP_SUBSCRIBE_EVENT", "conn_handle from subscribe=%d", conn_handle);
            break;

        case BLE_GAP_EVENT_ADV_COMPLETE:
            MODLOG_DFLT(INFO, "advertise complete; reason = %d", event->adv_complete.reason);
            start_advertisement();
            break;

        default:
            ESP_LOGD("BLE GAP Event", "Type: 0x%02X", event->type);
            break;
    }

    return 0;
}

static const struct ble_gatt_svc_def kBleServices[] = {
    {.type = BLE_GATT_SVC_TYPE_PRIMARY,
     .uuid = BLE_UUID16_DECLARE(GATT_MY_UUID),
     .characteristics = (struct ble_gatt_chr_def[]){
         {
             .uuid = BLE_UUID16_DECLARE(GATT_CURRENT_MEASURE_CTRL),
             .access_cb = current_ctrl_callback,
             .flags = BLE_GATT_CHR_F_WRITE,
         },
         {
             .uuid = BLE_UUID16_DECLARE(GATT_CURRENT_MEASURE),
             .access_cb = current_ctrl_callback,
             .val_handle = &ctx.notify_chr[kCurrent].handle,
             .flags = BLE_GATT_CHR_F_NOTIFY,
         },
         {
             .uuid = BLE_UUID16_DECLARE(GATT_VOLTAE_MEASURE_CTRL),
             .access_cb = voltage_ctrl_callback,
             .flags = BLE_GATT_CHR_F_WRITE,
         },
         {
             .uuid = BLE_UUID16_DECLARE(GATT_VOLTAE_MEASURE),
             .access_cb = voltage_ctrl_callback,
             .val_handle = &ctx.notify_chr[kVoltage].handle,
             .flags = BLE_GATT_CHR_F_NOTIFY,
         },
         {
             .uuid = BLE_UUID16_DECLARE(GATT_PWM),
             .access_cb = pwm_ctrl_callback,
             .flags = BLE_GATT_CHR_F_WRITE,
         },
         {
             .uuid = BLE_UUID16_DECLARE(GATT_RELAY),
             .access_cb = relay_ctrl_callback,
             .flags = BLE_GATT_CHR_F_WRITE,
         },
        {
             .uuid = BLE_UUID16_DECLARE(GATT_TEMPERATURE_CTRL),
             .access_cb = temperature_ctrl_callback,
             .flags = BLE_GATT_CHR_F_WRITE,
         },
         {
             .uuid = BLE_UUID16_DECLARE(GATT_TEMPERATURE),
             .access_cb = temperature_ctrl_callback,
             .val_handle = &ctx.notify_chr[kTemperature].handle,
             .flags = BLE_GATT_CHR_F_NOTIFY,
         },
         {
             0,
         },
     }},
    {
        0,
    },
};

static int init_ble_server(void) {
    ble_svc_gap_init();
    ble_svc_gatt_init();
    ble_svc_ans_init();

    int rc = ble_gatts_count_cfg(kBleServices);
    if (rc != 0) {
        ESP_LOGE("BLE GATT", "Initialization failed on: adjusting a host cofiguration");
        return rc;
    }

    rc = ble_gatts_add_svcs(kBleServices);
    if (rc != 0) {
        ESP_LOGE("BLE GATT", "Initialization failed:  heap exhaustion");
        return rc;
    }

    return 0;
}

static bool init_ble_controller_and_stack(void) {
    return nimble_port_init() == ESP_OK ? true : false;
}

static void on_reset(int reason) {
    MODLOG_DFLT(ERROR, "Resetting state; reason=%d\n", reason);
}

static void on_sync_host_and_controller(void) {
    int rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        MODLOG_DFLT(ERROR, "error determining address type; rc=%d\n", rc);
        return;
    }

    rc = ble_hs_util_ensure_addr(0);
    assert(rc == 0);

    start_advertisement();
}

static void on_register_resource(struct ble_gatt_register_ctxt *ctxt, void *arg) {
    char buf[BLE_UUID_STR_LEN];

    switch (ctxt->op) {
        case BLE_GATT_REGISTER_OP_SVC:
            MODLOG_DFLT(DEBUG, "registered service %s with handle=%d\n",
                        ble_uuid_to_str(ctxt->svc.svc_def->uuid, buf),
                        ctxt->svc.handle);
            break;

        case BLE_GATT_REGISTER_OP_CHR:
            MODLOG_DFLT(DEBUG,
                        "registering characteristic %s with "
                        "def_handle=%d val_handle=%d\n",
                        ble_uuid_to_str(ctxt->chr.chr_def->uuid, buf),
                        ctxt->chr.def_handle,
                        ctxt->chr.val_handle);
            break;

        case BLE_GATT_REGISTER_OP_DSC:
            MODLOG_DFLT(DEBUG, "registering descriptor %s with handle=%d\n",
                        ble_uuid_to_str(ctxt->dsc.dsc_def->uuid, buf),
                        ctxt->dsc.handle);
            break;

        default:
            assert(0);
            break;
    }
}

static void setup_callbacks(void) {
    ble_hs_cfg.reset_cb = on_reset;
    ble_hs_cfg.sync_cb = on_sync_host_and_controller;
    ble_hs_cfg.gatts_register_cb = on_register_resource;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
}

static bool init_nvs(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    return ret == ESP_OK ? true : false;
}

static void start_ble_server(void *param) {
    ESP_LOGI("BLE task", "BLE Host Task Started");

    nimble_port_run();
    nimble_port_freertos_deinit();
}

bool BLE_init(void) {
    if (init_nvs() == false)
        return false;

    if (init_ble_controller_and_stack() != true)
        return false;

    setup_callbacks();

    int rc = init_ble_server();
    if (rc != 0)
        return false;

    rc = ble_svc_gap_device_name_set("Controller-tester");
    if (rc != 0)
        return false;

    ble_store_config_init();
    nimble_port_freertos_init(start_ble_server);
    return true;
}

void BLE_setup_characteristic_callback(Characteristic name, CharacteristicCallback callback) {
    assert(name != kLastChr);
    assert(name != kLastMeasurementChr);

    if (name == kPWM) {
        assert(ctx.callback_pwm == NULL);
        ctx.callback_pwm = callback;
        return;
    }

    if (name == kRelay) {
        assert(ctx.callback_relay == NULL);
        ctx.callback_relay = callback;
        return;
    }

    assert(ctx.notify_chr[name].callback == NULL);
    ctx.notify_chr[name].callback = callback;
}

void BLE_update_value(Characteristic name, char *buffer) {
    // int rc;
    // ctx.notify_chr[name].value = value;
    strncpy(ctx.notify_chr[name].value, buffer, sizeof(ctx.notify_chr[name].value));
    struct os_mbuf *om;
    om = ble_hs_mbuf_from_flat(&ctx.notify_chr[name].value, sizeof(ctx.notify_chr[name].value));
    // rc =
    int rc = ble_gatts_notify_custom(conn_handle, ctx.notify_chr[name].handle, om);
    if (rc != 0) {
        ESP_LOGE(__func__, "error notifying; rc=%d", rc);
        return;
    }
}
