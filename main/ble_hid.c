#include "ble_hid.h"

#include <string.h>

#include "esp_bt.h"
#include "esp_check.h"
#include "esp_hid_common.h"
#include "esp_hidd.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "host/ble_gap.h"
#include "host/ble_hs.h"
#include "host/ble_hs_adv.h"
#include "host/ble_store.h"
#include "host/util/util.h"
#include "nimble/ble.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "services/gap/ble_svc_gap.h"
#include "store/config/ble_store_config.h"

#define BLE_HID_DEVICE_NAME "vibestick Keyboard"
#define BLE_HID_REPORT_ID   1
#define BLE_HID_MAP_INDEX   0

static const char *TAG = "vox_ble_hid";
static esp_hidd_dev_t *s_hid_dev;
static volatile bool s_connected;
static volatile bool s_encrypted;
static volatile bool s_initialized;
static volatile uint32_t s_generation = 1;
static volatile bool s_host_ready;
static volatile bool s_advertising;
static uint8_t s_own_addr_type;
static TaskHandle_t s_adv_task_handle;

void ble_store_config_init(void);

static const uint8_t s_keyboard_report_map[] = {
    0x05, 0x01,       // Usage Page (Generic Desktop)
    0x09, 0x06,       // Usage (Keyboard)
    0xA1, 0x01,       // Collection (Application)
    0x85, BLE_HID_REPORT_ID,
    0x05, 0x07,       // Usage Page (Keyboard/Keypad)
    0x19, 0xE0,
    0x29, 0xE7,
    0x15, 0x00,
    0x25, 0x01,
    0x75, 0x01,
    0x95, 0x08,
    0x81, 0x02,       // Modifier input
    0x95, 0x01,
    0x75, 0x08,
    0x81, 0x03,       // Reserved byte
    0x95, 0x05,
    0x75, 0x01,
    0x05, 0x08,       // Usage Page (LEDs)
    0x19, 0x01,
    0x29, 0x05,
    0x91, 0x02,
    0x95, 0x01,
    0x75, 0x03,
    0x91, 0x03,
    0x95, 0x06,
    0x75, 0x08,
    0x15, 0x00,
    0x25, 0x73,
    0x05, 0x07,
    0x19, 0x00,
    0x29, 0x73,
    0x81, 0x00,       // Six-key rollover input
    0xC0,
};

static esp_hid_raw_report_map_t s_report_maps[] = {
    {
        .data = s_keyboard_report_map,
        .len = sizeof(s_keyboard_report_map),
    },
};

static esp_hid_device_config_t s_hid_config = {
    .vendor_id = 0x303A,
    .product_id = 0x8001,
    .version = 0x0100,
    .device_name = BLE_HID_DEVICE_NAME,
    .manufacturer_name = "voxstick",
    .serial_number = "0001",
    .report_maps = s_report_maps,
    .report_maps_len = 1,
};

static int start_advertising(void);

static void request_advertising(void)
{
    if (s_adv_task_handle != NULL) {
        xTaskNotifyGive(s_adv_task_handle);
    }
}

static void advertising_task(void *arg)
{
    (void)arg;
    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        vTaskDelay(pdMS_TO_TICKS(100));
        for (int attempt = 0; attempt < 20; attempt++) {
            if (s_connected) {
                break;
            }
            if (s_advertising ||
                (s_host_ready && start_advertising() == 0)) {
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}

static int gap_event_cb(struct ble_gap_event *event, void *arg)
{
    (void)arg;

    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        s_advertising = false;
        if (event->connect.status != 0) {
            ESP_LOGW(TAG, "BLE connection failed: %d", event->connect.status);
            request_advertising();
            return 0;
        }
        s_generation++;
        s_connected = true;
        s_encrypted = false;
        ESP_LOGI(TAG, "BLE host connected");
        int rc = ble_gap_security_initiate(event->connect.conn_handle);
        if (rc != 0) {
            ESP_LOGE(TAG, "BLE security initiation failed: %d", rc);
        }
        return 0;

    case BLE_GAP_EVENT_DISCONNECT:
        s_advertising = false;
        s_generation++;
        s_connected = false;
        s_encrypted = false;
        ESP_LOGI(TAG, "BLE host disconnected: %d", event->disconnect.reason);
        request_advertising();
        return 0;

    case BLE_GAP_EVENT_ENC_CHANGE: {
        struct ble_gap_conn_desc desc = {0};
        if (event->enc_change.status == 0 &&
            ble_gap_conn_find(event->enc_change.conn_handle, &desc) == 0) {
            s_encrypted = desc.sec_state.encrypted;
        } else {
            s_encrypted = false;
        }
        ESP_LOGI(TAG, "BLE encryption %s", s_encrypted ? "ready" : "off");
        return 0;
    }

    case BLE_GAP_EVENT_REPEAT_PAIRING: {
        struct ble_gap_conn_desc desc = {0};
        if (ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc) == 0) {
            ble_store_util_delete_peer(&desc.peer_id_addr);
        }
        return BLE_GAP_REPEAT_PAIRING_RETRY;
    }

    case BLE_GAP_EVENT_ADV_COMPLETE:
        s_advertising = false;
        if (!s_connected) {
            request_advertising();
        }
        return 0;

    default:
        return 0;
    }
}

static int start_advertising(void)
{
    if (!s_host_ready || s_connected) {
        return BLE_HS_EBUSY;
    }
    struct ble_hs_adv_fields fields = {0};
    ble_uuid16_t hid_service = BLE_UUID16_INIT(0x1812);

    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.appearance = ESP_HID_APPEARANCE_KEYBOARD;
    fields.appearance_is_present = 1;
    fields.name = (uint8_t *)BLE_HID_DEVICE_NAME;
    fields.name_len = strlen(BLE_HID_DEVICE_NAME);
    fields.name_is_complete = 1;
    fields.uuids16 = &hid_service;
    fields.num_uuids16 = 1;
    fields.uuids16_is_complete = 1;

    int rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "set advertising fields failed: %d", rc);
        return rc;
    }

    struct ble_gap_adv_params params = {0};
    params.conn_mode = BLE_GAP_CONN_MODE_UND;
    params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    params.itvl_min = BLE_GAP_ADV_ITVL_MS(30);
    params.itvl_max = BLE_GAP_ADV_ITVL_MS(50);

    rc = ble_gap_adv_start(s_own_addr_type, NULL, BLE_HS_FOREVER,
                           &params, gap_event_cb, NULL);
    if (rc == 0) {
        s_advertising = true;
        ESP_LOGI(TAG, "advertising as '%s'", BLE_HID_DEVICE_NAME);
    } else {
        ESP_LOGE(TAG, "start advertising failed: %d", rc);
    }
    return rc;
}

static void start_after_host_sync(void)
{
    int rc = ble_hs_util_ensure_addr(0);
    if (rc == 0) {
        rc = ble_hs_id_infer_auto(0, &s_own_addr_type);
    }
    if (rc == 0) {
        rc = ble_svc_gap_device_name_set(BLE_HID_DEVICE_NAME);
    }
    if (rc != 0) {
        ESP_LOGE(TAG, "BLE identity setup failed: %d", rc);
        return;
    }
    s_host_ready = true;
    request_advertising();
}

static void hidd_event_cb(void *arg, esp_event_base_t base,
                          int32_t id, void *event_data)
{
    (void)arg;
    (void)base;
    (void)event_data;

    if (id == ESP_HIDD_START_EVENT) {
        s_initialized = true;
        ESP_LOGI(TAG, "BLE HID service started");
        start_after_host_sync();
    } else if (id == ESP_HIDD_CONNECT_EVENT) {
        s_connected = true;
    } else if (id == ESP_HIDD_DISCONNECT_EVENT) {
        s_connected = false;
        s_encrypted = false;
        request_advertising();
    }
}

static void host_task(void *arg)
{
    (void)arg;
    nimble_port_run();
    nimble_port_freertos_deinit();
}

esp_err_t vox_ble_hid_init(bool clear_bonds)
{
    esp_bt_controller_config_t controller_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_err_t ret = esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        return ret;
    }
    ESP_RETURN_ON_ERROR(esp_bt_controller_init(&controller_cfg), TAG,
                        "controller init");
    ESP_RETURN_ON_ERROR(esp_bt_controller_enable(ESP_BT_MODE_BLE), TAG,
                        "controller enable");
    ESP_RETURN_ON_ERROR(esp_nimble_init(), TAG, "NimBLE init");

    ble_hs_cfg.reset_cb = NULL;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
    ble_hs_cfg.sm_io_cap = BLE_SM_IO_CAP_NO_IO;
    ble_hs_cfg.sm_bonding = 1;
    ble_hs_cfg.sm_mitm = 0;
    ble_hs_cfg.sm_sc = 1;
    ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ENC |
                                 BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC |
                                   BLE_SM_PAIR_KEY_DIST_ID;
    ble_store_config_init();
    if (clear_bonds) {
        ESP_RETURN_ON_FALSE(ble_store_clear() == 0, ESP_FAIL, TAG,
                            "clear BLE bonds");
        ESP_LOGW(TAG, "BLE bonds cleared; pair '%s' again",
                 BLE_HID_DEVICE_NAME);
    }

    int rc = ble_svc_gap_device_name_set(BLE_HID_DEVICE_NAME);
    if (rc != 0) {
        return ESP_FAIL;
    }
    ESP_RETURN_ON_ERROR(
        esp_hidd_dev_init(&s_hid_config, ESP_HID_TRANSPORT_BLE,
                          hidd_event_cb, &s_hid_dev),
        TAG, "HID service init");
    if (xTaskCreate(advertising_task, "ble_adv", 3072, NULL, 4,
                    &s_adv_task_handle) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    ESP_RETURN_ON_ERROR(esp_nimble_enable(host_task), TAG, "host task");
    return ESP_OK;
}

bool vox_ble_hid_ready(void)
{
    return s_initialized && s_connected && s_encrypted &&
           s_hid_dev != NULL && esp_hidd_dev_connected(s_hid_dev);
}

uint32_t vox_ble_hid_generation(void)
{
    return s_generation;
}

esp_err_t vox_ble_hid_send_report(uint32_t generation,
                                  uint8_t modifier, uint8_t keycode)
{
    if (generation != s_generation || !vox_ble_hid_ready()) {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t report[8] = {0};
    report[0] = modifier;
    report[2] = keycode;
    return esp_hidd_dev_input_set(s_hid_dev, BLE_HID_MAP_INDEX,
                                  BLE_HID_REPORT_ID, report, sizeof(report));
}
