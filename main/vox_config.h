#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define VOX_CONFIG_PROTOCOL_VERSION        2
#define VOX_CONFIG_PROTOCOL_VERSION_LEGACY 1

#define VOX_CONFIG_CMD_GET   1
#define VOX_CONFIG_CMD_SET   2
#define VOX_CONFIG_CMD_RESET 3

#define VOX_CONFIG_STATUS_OK       0
#define VOX_CONFIG_STATUS_BAD_REQ  1
#define VOX_CONFIG_STATUS_INVALID  2
#define VOX_CONFIG_STATUS_STORAGE  3

typedef struct __attribute__((packed)) {
    uint8_t modifier;
    uint8_t keycode;
} vox_hid_action_t;

typedef struct __attribute__((packed)) {
    uint8_t  flat_mute_enabled;
    uint8_t  reserved;
    vox_hid_action_t btn_a_single;
    vox_hid_action_t btn_a_double;
    vox_hid_action_t btn_a_long;
    vox_hid_action_t btn_b_single;
    vox_hid_action_t btn_b_double;
    vox_hid_action_t btn_b_long;
    uint16_t long_press_ms;
    uint32_t reserved2;
} vox_config_wire_t;

typedef struct __attribute__((packed)) {
    uint8_t  flat_mute_enabled;
    uint8_t  tap_modifier;
    uint8_t  tap_keycode;
    uint8_t  hold_modifier;
    uint8_t  hold_keycode;
    uint8_t  reserved;
    uint16_t long_press_ms;
    uint32_t reserved2;
} vox_config_v1_wire_t;

_Static_assert(sizeof(vox_hid_action_t) == 2, "HID action wire size changed");
_Static_assert(sizeof(vox_config_v1_wire_t) == 12, "v1 config wire size changed");
_Static_assert(sizeof(vox_config_wire_t) == 20, "v2 config wire size changed");
_Static_assert(offsetof(vox_config_wire_t, flat_mute_enabled) == 0,
               "v2 flat-mute offset changed");
_Static_assert(offsetof(vox_config_wire_t, btn_a_single) == 2,
               "v2 BtnA single offset changed");
_Static_assert(offsetof(vox_config_wire_t, btn_a_double) == 4,
               "v2 BtnA double offset changed");
_Static_assert(offsetof(vox_config_wire_t, btn_a_long) == 6,
               "v2 BtnA long offset changed");
_Static_assert(offsetof(vox_config_wire_t, btn_b_single) == 8,
               "v2 BtnB single offset changed");
_Static_assert(offsetof(vox_config_wire_t, btn_b_double) == 10,
               "v2 BtnB double offset changed");
_Static_assert(offsetof(vox_config_wire_t, btn_b_long) == 12,
               "v2 BtnB long offset changed");
_Static_assert(offsetof(vox_config_wire_t, long_press_ms) == 14,
               "v2 long-press offset changed");
_Static_assert(offsetof(vox_config_wire_t, reserved2) == 16,
               "v2 reserved offset changed");

void vox_config_init(void);
void vox_config_get(vox_config_wire_t *out);
bool vox_config_flat_mute_enabled(void);
esp_err_t vox_config_set(vox_config_wire_t const *cfg);
esp_err_t vox_config_reset(void);

#ifdef __cplusplus
}
#endif
