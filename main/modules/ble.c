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

#define CONFIG_LOG_DEFAULT_LEVEL    DEBUG
#include <esp_log.h>
#include <services/ans/ble_svc_ans.h>
#include <host/util/util.h>
#include <nimble/nimble_port.h>
#include <nimble/nimble_port_freertos.h>
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

static uint8_t own_addr_type = 0;
static int on_ble_gap_event(struct ble_gap_event *event, void *arg);

static struct {
    MeasurementChr name;
    uint16_t value;
    uint16_t handle;

} ctx[] = {
    {kVoltage, 0, 0},
    {kCurrent, 0, 0},
};
static uint16_t conn_handle;

// static bool device_connected;
// static uint16_t voltage_handle;
// static uint16_t current_handle;

static int current_ctrl_callback(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg) {
    int rc = os_mbuf_append(ctxt->om, &ctx[kCurrent].value, sizeof(ctx[kCurrent].value));
    return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

static int voltage_ctrl_callback(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg) {
    return 0;
}

static int pwm_ctrl_callback(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg) {
    return 0;
}

static int relay_ctrl_callback(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg) {
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

    advertisement_fields.uuids16 = (ble_uuid16_t[]){
        BLE_UUID16_INIT(GATT_SVR_SVC_ALERT_UUID)};
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
            ESP_LOGI("BLE GAP Event", "Connection %s; status=%d ",
                     event->connect.status == 0 ? "established" : "failed",
                     event->connect.status);

            ESP_LOGI("BLE GAP Event", "Connected");

            if (event->connect.status == 0) {
                int rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
                assert(rc == 0);
            }

            if (event->connect.status != 0)
                start_advertisement();

            conn_handle = event->connect.conn_handle;
            break;

        case BLE_GAP_EVENT_DISCONNECT:
            MODLOG_DFLT(INFO, "disconnect; reason=%d ", event->disconnect.reason);
            ESP_LOGI("BLE GAP Event", "Disconnected");
            start_advertisement();
            break;

        case BLE_GAP_EVENT_ADV_COMPLETE:
            MODLOG_DFLT(INFO, "advertise complete; reason=%d", event->adv_complete.reason);
            start_advertisement();
            break;

        default:
            ESP_LOGI("BLE GAP Event", "Type: 0x%02X", event->type);
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
             .val_handle = &ctx[kCurrent].handle,
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
             .val_handle = &ctx[kVoltage].handle,
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

bool BLE_init(void) {
    if (init_nvs() == false)
        return false;

    if (init_ble_controller_and_stack() != true)
        return false;

    setup_callbacks();

    int rc = init_ble_server();
    ESP_LOGI(__func__, "ble server");
    if (rc != 0)
        return false;

    rc = ble_svc_gap_device_name_set("Controller-tester");
    if (rc != 0)
        return false;

    ble_store_config_init();
    nimble_port_run();
    nimble_port_freertos_deinit();
    return true;
}

void BLE_UpdateValue(MeasurementChr name, uint16_t value) {
    // int rc;
    struct os_mbuf *om;
    om = ble_hs_mbuf_from_flat(&ctx[name].value, sizeof(ctx[name].value));
    // rc =
    ble_gattc_notify_custom(conn_handle, ctx[name].handle, om);
}

// void notify_over_control_chr(int16_t conn_handle, uint32_t data){
//     struct os_mbuf *om;
//     if(conn_handle > -1){
//         om = ble_hs_mbuf_from_flat(&data, sizeof(&data));
//         ESP_LOGI(TAG, "Notifying conn=%d", conn_handle);
//         int rc = ble_gatts_notify_custom((uint16_t)conn_handle, control_notif_handle, om);
//         if (rc != 0) {
//             ESP_LOGE(TAG, "error notifying; rc=%d", rc);
//             return;
//         }
//     }
// }