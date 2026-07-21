#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define VOX_CONFIG_PROTOCOL_VERSION     5
#define VOX_CONFIG_PROTOCOL_VERSION_V4  4
#define VOX_CONFIG_PROTOCOL_VERSION_V3  3
#define VOX_CONFIG_PROTOCOL_VERSION_V2  2
#define VOX_CONFIG_PROTOCOL_VERSION_V1  1

#define VOX_CONFIG_CMD_GET   1
#define VOX_CONFIG_CMD_SET   2
#define VOX_CONFIG_CMD_RESET 3

#define VOX_CONFIG_STATUS_OK       0
#define VOX_CONFIG_STATUS_BAD_REQ  1
#define VOX_CONFIG_STATUS_INVALID  2
#define VOX_CONFIG_STATUS_STORAGE  3

#define VOX_CONFIG_FLAT_THRESHOLD_DEFAULT_LSB 10500
#define VOX_CONFIG_FLAT_THRESHOLD_MIN_LSB      5600
#define VOX_CONFIG_FLAT_THRESHOLD_MAX_LSB     14200

typedef struct __attribute__((packed)) {
    uint8_t modifier;
    uint8_t keycode;
    uint8_t repeat_count;
} vox_hid_action_t;

typedef struct __attribute__((packed)) {
    uint8_t modifier;
    uint8_t keycode;
} vox_hid_action_v2_t;

typedef struct __attribute__((packed)) {
    uint8_t  flat_mute_enabled;
    uint8_t  reserved;
    vox_hid_action_t btn_a_single;
    vox_hid_action_t btn_a_double;
    vox_hid_action_t btn_a_long;
    vox_hid_action_t btn_b_single;
    vox_hid_action_t btn_b_double;
    vox_hid_action_t btn_b_long;
    vox_hid_action_t shake;
    uint16_t long_press_ms;
    uint32_t reserved2;
    uint16_t flat_mute_threshold_lsb;
} vox_config_wire_t;

typedef struct __attribute__((packed)) {
    uint8_t  flat_mute_enabled;
    uint8_t  reserved;
    vox_hid_action_t btn_a_single;
    vox_hid_action_t btn_a_double;
    vox_hid_action_t btn_a_long;
    vox_hid_action_t btn_b_single;
    vox_hid_action_t btn_b_double;
    vox_hid_action_t btn_b_long;
    vox_hid_action_t shake;
    uint16_t long_press_ms;
    uint32_t reserved2;
} vox_config_v4_wire_t;

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
} vox_config_v3_wire_t;

typedef struct __attribute__((packed)) {
    uint8_t  flat_mute_enabled;
    uint8_t  reserved;
    vox_hid_action_v2_t btn_a_single;
    vox_hid_action_v2_t btn_a_double;
    vox_hid_action_v2_t btn_a_long;
    vox_hid_action_v2_t btn_b_single;
    vox_hid_action_v2_t btn_b_double;
    vox_hid_action_v2_t btn_b_long;
    uint16_t long_press_ms;
    uint32_t reserved2;
} vox_config_v2_wire_t;

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

_Static_assert(sizeof(vox_hid_action_t) == 3, "HID action wire size changed");
_Static_assert(sizeof(vox_hid_action_v2_t) == 2, "v2 HID action size changed");
_Static_assert(sizeof(vox_config_v1_wire_t) == 12, "v1 config wire size changed");
_Static_assert(sizeof(vox_config_v2_wire_t) == 20, "v2 config wire size changed");
_Static_assert(sizeof(vox_config_v3_wire_t) == 26, "v3 config wire size changed");
_Static_assert(sizeof(vox_config_v4_wire_t) == 29, "v4 config wire size changed");
_Static_assert(sizeof(vox_config_wire_t) == 31, "v5 config wire size changed");
_Static_assert(offsetof(vox_config_wire_t, flat_mute_enabled) == 0,
               "v5 flat-mute offset changed");
_Static_assert(offsetof(vox_config_wire_t, btn_a_single) == 2,
               "v5 BtnA single offset changed");
_Static_assert(offsetof(vox_config_wire_t, btn_a_double) == 5,
               "v5 BtnA double offset changed");
_Static_assert(offsetof(vox_config_wire_t, btn_a_long) == 8,
               "v5 BtnA long offset changed");
_Static_assert(offsetof(vox_config_wire_t, btn_b_single) == 11,
               "v5 BtnB single offset changed");
_Static_assert(offsetof(vox_config_wire_t, btn_b_double) == 14,
               "v5 BtnB double offset changed");
_Static_assert(offsetof(vox_config_wire_t, btn_b_long) == 17,
               "v5 BtnB long offset changed");
_Static_assert(offsetof(vox_config_wire_t, shake) == 20,
               "v5 shake offset changed");
_Static_assert(offsetof(vox_config_wire_t, long_press_ms) == 23,
               "v5 long-press offset changed");
_Static_assert(offsetof(vox_config_wire_t, reserved2) == 25,
               "v5 reserved offset changed");
_Static_assert(offsetof(vox_config_wire_t, flat_mute_threshold_lsb) == 29,
               "v5 flat threshold offset changed");
_Static_assert(offsetof(vox_config_wire_t, flat_mute_threshold_lsb) ==
                   sizeof(vox_config_v4_wire_t),
               "v4 config must remain a v5 prefix");

void vox_config_init(void);
void vox_config_get(vox_config_wire_t *out);
bool vox_config_flat_mute_enabled(void);
uint16_t vox_config_flat_mute_threshold_lsb(void);
esp_err_t vox_config_set(vox_config_wire_t const *cfg);
esp_err_t vox_config_reset(void);

#ifdef __cplusplus
}
#endif
