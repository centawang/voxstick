#include "hid_transport.h"

#include "ble_hid.h"
#include "class/hid/hid_device.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "tusb.h"

#define USB_HID_REPORT_ID 1
#define BLE_REPORT_GAP_MS 20

static const char *TAG = "hid_transport";
static SemaphoreHandle_t s_hid_mutex;
static volatile uint32_t s_usb_generation = 1;
static volatile bool s_usb_mounted;

void tud_mount_cb(void)
{
    s_usb_generation++;
    s_usb_mounted = true;
    ESP_LOGI(TAG, "USB mounted; HID route active");
}

void tud_umount_cb(void)
{
    s_usb_generation++;
    s_usb_mounted = false;
    ESP_LOGI(TAG, "USB unmounted; BLE fallback eligible");
}

static bool usb_submit_report(vox_hid_target_t target,
                              uint8_t modifier, uint8_t keycode,
                              uint32_t timeout_ms)
{
    uint8_t keys[6] = {keycode};
    TickType_t start = xTaskGetTickCount();
    do {
        if (!s_usb_mounted || target.generation != s_usb_generation) {
            return false;
        }
        if (tud_hid_ready() &&
            tud_hid_keyboard_report(USB_HID_REPORT_ID, modifier, keys)) {
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    } while (xTaskGetTickCount() - start < pdMS_TO_TICKS(timeout_ms));
    return false;
}

static bool ble_submit_report(vox_hid_target_t target,
                              uint8_t modifier, uint8_t keycode,
                              uint32_t timeout_ms)
{
    TickType_t start = xTaskGetTickCount();
    do {
        if (!vox_ble_hid_ready() ||
            target.generation != vox_ble_hid_generation()) {
            return false;
        }
        if (vox_ble_hid_send_report(target.generation,
                                    modifier, keycode) == ESP_OK) {
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    } while (xTaskGetTickCount() - start < pdMS_TO_TICKS(timeout_ms));
    return false;
}

static bool submit_report(vox_hid_target_t target,
                          uint8_t modifier, uint8_t keycode,
                          uint32_t timeout_ms)
{
    if (target.route == VOX_HID_ROUTE_USB) {
        return usb_submit_report(target, modifier, keycode, timeout_ms);
    }
    if (target.route == VOX_HID_ROUTE_BLE) {
        return ble_submit_report(target, modifier, keycode, timeout_ms);
    }
    return false;
}

esp_err_t vox_hid_transport_init(void)
{
    s_hid_mutex = xSemaphoreCreateMutex();
    return s_hid_mutex != NULL ? ESP_OK : ESP_ERR_NO_MEM;
}

vox_hid_target_t vox_hid_transport_target(vox_hid_route_t route)
{
    vox_hid_target_t target = {
        .route = route,
        .generation = 0,
    };
    if (route == VOX_HID_ROUTE_USB) {
        target.generation = s_usb_generation;
    } else if (route == VOX_HID_ROUTE_BLE) {
        target.generation = vox_ble_hid_generation();
    }
    return target;
}

vox_hid_target_t vox_hid_transport_select(void)
{
    if (s_usb_mounted) {
        return vox_hid_transport_target(VOX_HID_ROUTE_USB);
    }
    if (vox_ble_hid_ready()) {
        return vox_hid_transport_target(VOX_HID_ROUTE_BLE);
    }
    return vox_hid_transport_target(VOX_HID_ROUTE_NONE);
}

bool vox_hid_transport_ready(vox_hid_target_t target)
{
    if (target.route == VOX_HID_ROUTE_USB) {
        return s_usb_mounted && target.generation == s_usb_generation;
    }
    if (target.route == VOX_HID_ROUTE_BLE) {
        return vox_ble_hid_ready() &&
               target.generation == vox_ble_hid_generation();
    }
    return false;
}

const char *vox_hid_transport_name(vox_hid_route_t route)
{
    if (route == VOX_HID_ROUTE_USB) {
        return "USB";
    }
    if (route == VOX_HID_ROUTE_BLE) {
        return "BLE";
    }
    return "none";
}

bool vox_hid_transport_send_tap(vox_hid_target_t target,
                                uint8_t modifier, uint8_t keycode)
{
    if ((modifier == 0 && keycode == 0) ||
        !vox_hid_transport_ready(target) || s_hid_mutex == NULL) {
        return false;
    }
    if (xSemaphoreTake(s_hid_mutex, pdMS_TO_TICKS(250)) != pdTRUE) {
        return false;
    }

    bool down_sent = submit_report(target, modifier, keycode, 150);
    bool up_sent = false;
    if (down_sent) {
        if (target.route == VOX_HID_ROUTE_BLE) {
            vTaskDelay(pdMS_TO_TICKS(BLE_REPORT_GAP_MS));
        }
        up_sent = submit_report(target, 0, 0, 250);
    }

    xSemaphoreGive(s_hid_mutex);
    if (down_sent && !up_sent) {
        ESP_LOGW(TAG, "%s HID key release timed out",
                 vox_hid_transport_name(target.route));
    }
    return down_sent && up_sent;
}