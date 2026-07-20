// Strong TinyUSB vendor-class callbacks for host-controlled ROM download and
// browser-based configuration.
//
// Keep this in a separate translation unit that does not include tusb.h:
// vendor_device.h declares tud_vendor_rx_cb as weak, and GCC carries that
// attribute onto definitions in the same translation unit. A strong symbol here
// makes sure TinyUSB calls our recovery callback instead of a weak no-op.

#include <stdint.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/soc.h"
#include "vox_config.h"

#define VENDOR_DOWNLOAD_MAGIC "VOXSTICK_DOWNLOAD"
#define CFG_REQ_MAGIC "VXCF"
#define CFG_RESP_MAGIC "VXCR"
#define CFG_HEADER_LEN 8

static const char *TAG = "voxstick";

uint32_t tud_vendor_n_write(uint8_t itf, void const *buffer, uint32_t bufsize);
uint32_t tud_vendor_n_write_flush(uint8_t itf);
void tud_vendor_n_read_flush(uint8_t itf);

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

static uint16_t read_u16_le(uint8_t const *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static void write_u16_le(uint8_t *p, uint16_t value)
{
    p[0] = (uint8_t)(value & 0xff);
    p[1] = (uint8_t)(value >> 8);
}

static vox_config_v1_wire_t config_v1_from_current(vox_config_wire_t const *cfg)
{
    return (vox_config_v1_wire_t) {
        .flat_mute_enabled = cfg->flat_mute_enabled,
        .tap_modifier = cfg->btn_a_single.modifier,
        .tap_keycode = cfg->btn_a_single.keycode,
        .hold_modifier = cfg->btn_a_long.modifier,
        .hold_keycode = cfg->btn_a_long.keycode,
        .reserved = 0,
        .long_press_ms = cfg->long_press_ms,
        .reserved2 = 0,
    };
}

static void write_config_response(uint8_t itf, uint8_t protocol_version,
                                  uint8_t status,
                                  vox_config_wire_t const *cfg)
{
    uint8_t resp[CFG_HEADER_LEN + sizeof(vox_config_wire_t)] = {
        'V', 'X', 'C', 'R',
        protocol_version,
        status,
        0,
        0,
    };
    uint16_t payload_len = 0;

    if (status == VOX_CONFIG_STATUS_OK && cfg != NULL) {
        if (protocol_version == VOX_CONFIG_PROTOCOL_VERSION_LEGACY) {
            vox_config_v1_wire_t legacy = config_v1_from_current(cfg);
            payload_len = sizeof(legacy);
            memcpy(resp + CFG_HEADER_LEN, &legacy, payload_len);
        } else {
            payload_len = sizeof(*cfg);
            memcpy(resp + CFG_HEADER_LEN, cfg, payload_len);
        }
    }
    write_u16_le(resp + 6, payload_len);

    (void)tud_vendor_n_write(itf, resp, CFG_HEADER_LEN + payload_len);
    (void)tud_vendor_n_write_flush(itf);
}

static uint8_t config_status_from_error(esp_err_t ret)
{
    if (ret == ESP_ERR_INVALID_ARG) {
        return VOX_CONFIG_STATUS_INVALID;
    }
    return VOX_CONFIG_STATUS_STORAGE;
}

static bool handle_config_packet(uint8_t itf,
                                 uint8_t const *buffer,
                                 uint16_t bufsize)
{
    if (buffer == NULL || bufsize < strlen(CFG_REQ_MAGIC) ||
        memcmp(buffer, CFG_REQ_MAGIC, strlen(CFG_REQ_MAGIC)) != 0) {
        return false;
    }

    if (bufsize < CFG_HEADER_LEN) {
        write_config_response(itf, VOX_CONFIG_PROTOCOL_VERSION,
                              VOX_CONFIG_STATUS_BAD_REQ, NULL);
        return true;
    }

    uint8_t version = buffer[4];
    uint8_t command = buffer[5];
    uint16_t payload_len = read_u16_le(buffer + 6);
    if ((version != VOX_CONFIG_PROTOCOL_VERSION &&
         version != VOX_CONFIG_PROTOCOL_VERSION_LEGACY) ||
        payload_len > bufsize - CFG_HEADER_LEN ||
        CFG_HEADER_LEN + payload_len != bufsize) {
        write_config_response(itf, version, VOX_CONFIG_STATUS_BAD_REQ, NULL);
        return true;
    }

    vox_config_wire_t cfg = {0};
    esp_err_t ret = ESP_OK;
    switch (command) {
    case VOX_CONFIG_CMD_GET:
        if (payload_len != 0) {
            write_config_response(itf, version,
                                  VOX_CONFIG_STATUS_BAD_REQ, NULL);
            return true;
        }
        vox_config_get(&cfg);
        write_config_response(itf, version, VOX_CONFIG_STATUS_OK, &cfg);
        return true;

    case VOX_CONFIG_CMD_SET:
        if (version == VOX_CONFIG_PROTOCOL_VERSION_LEGACY) {
            if (payload_len != sizeof(vox_config_v1_wire_t)) {
                write_config_response(itf, version,
                                      VOX_CONFIG_STATUS_BAD_REQ, NULL);
                return true;
            }
            vox_config_v1_wire_t legacy = {0};
            memcpy(&legacy, buffer + CFG_HEADER_LEN, sizeof(legacy));
            vox_config_get(&cfg);
            cfg.flat_mute_enabled = legacy.flat_mute_enabled;
            cfg.btn_a_single.modifier = legacy.tap_modifier;
            cfg.btn_a_single.keycode = legacy.tap_keycode;
            cfg.btn_a_long.modifier = legacy.hold_modifier;
            cfg.btn_a_long.keycode = legacy.hold_keycode;
            cfg.long_press_ms = legacy.long_press_ms;
        } else {
            if (payload_len != sizeof(cfg)) {
                write_config_response(itf, version,
                                      VOX_CONFIG_STATUS_BAD_REQ, NULL);
                return true;
            }
            memcpy(&cfg, buffer + CFG_HEADER_LEN, sizeof(cfg));
        }
        ret = vox_config_set(&cfg);
        if (ret != ESP_OK) {
            write_config_response(itf, version,
                                  config_status_from_error(ret), NULL);
            return true;
        }
        vox_config_get(&cfg);
        write_config_response(itf, version, VOX_CONFIG_STATUS_OK, &cfg);
        return true;

    case VOX_CONFIG_CMD_RESET:
        if (payload_len != 0) {
            write_config_response(itf, version,
                                  VOX_CONFIG_STATUS_BAD_REQ, NULL);
            return true;
        }
        ret = vox_config_reset();
        if (ret != ESP_OK) {
            write_config_response(itf, version,
                                  config_status_from_error(ret), NULL);
            return true;
        }
        vox_config_get(&cfg);
        write_config_response(itf, version, VOX_CONFIG_STATUS_OK, &cfg);
        return true;

    default:
        write_config_response(itf, version, VOX_CONFIG_STATUS_BAD_REQ, NULL);
        return true;
    }
}

void tud_vendor_rx_cb(uint8_t itf, uint8_t const *buffer, uint16_t bufsize)
{
    static const char magic[] = VENDOR_DOWNLOAD_MAGIC;
    const size_t magic_len = sizeof(magic) - 1;

    if (buffer == NULL || bufsize == 0) {
        return;
    }

    if (bufsize >= magic_len) {
        for (uint16_t off = 0; off + magic_len <= bufsize; off++) {
            if (memcmp(buffer + off, magic, magic_len) == 0) {
                enter_download_from_vendor_bulk();
                return;
            }
        }
    }

    (void)handle_config_packet(itf, buffer, bufsize);

    // TinyUSB copies each received packet into its buffered vendor RX FIFO
    // before invoking this callback. If we do not drain it, fewer than one
    // 64-byte USB packet remains free and the bulk OUT endpoint is never
    // armed again, so the next WebUSB transferOut() waits until it times out.
    tud_vendor_n_read_flush(itf);
}
