// Strong TinyUSB vendor-class callbacks for host-controlled ROM download.
//
// Keep this in a separate translation unit that does not include tusb.h:
// vendor_device.h declares tud_vendor_rx_cb as weak, and GCC carries that
// attribute onto definitions in the same translation unit. A strong symbol here
// makes sure TinyUSB calls our recovery callback instead of a weak no-op.

#include <stdint.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/soc.h"

#define VENDOR_DOWNLOAD_MAGIC "VOXSTICK_DOWNLOAD"

static const char *TAG = "voxstick";

void voxstick_force_link_usb_recovery(void)
{
}

static void enter_download_from_vendor_bulk(void)
{
    ESP_LOGW(TAG, "USB vendor bulk magic - rebooting into ROM download mode");
    REG_SET_BIT(RTC_CNTL_OPTION1_REG, RTC_CNTL_FORCE_DOWNLOAD_BOOT);
    vTaskDelay(pdMS_TO_TICKS(150));
    esp_restart();
}

void tud_vendor_rx_cb(uint8_t itf, uint8_t const *buffer, uint16_t bufsize)
{
    (void)itf;
    static const char magic[] = VENDOR_DOWNLOAD_MAGIC;
    const size_t magic_len = sizeof(magic) - 1;

    if (buffer == NULL || bufsize < magic_len) {
        return;
    }

    for (uint16_t off = 0; off + magic_len <= bufsize; off++) {
        if (memcmp(buffer + off, magic, magic_len) == 0) {
            enter_download_from_vendor_bulk();
            return;
        }
    }
}
