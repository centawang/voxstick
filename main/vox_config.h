#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define VOX_CONFIG_PROTOCOL_VERSION     7
#define VOX_CONFIG_PROTOCOL_VERSION_V6  6
#define VOX_CONFIG_PROTOCOL_VERSION_V5  5
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
#define VOX_CONFIG_FLAT_TRANSITION_DEFAULT_MS   300
#define VOX_CONFIG_FLAT_TRANSITION_MIN_MS        50
#define VOX_CONFIG_FLAT_TRANSITION_MAX_MS      2000

#define VOX_DOG_STYLE_PIXEL       0
#define VOX_DOG_STYLE_SHIBA       1
#define VOX_DOG_STYLE_CORGI       2
#define VOX_DOG_STYLE_LABRADOR    3
#define VOX_DOG_STYLE_COLLIE      4
#define VOX_DOG_STYLE_COUNT       5
#define VOX_DOG_STYLE_DEFAULT     VOX_DOG_STYLE_PIXEL

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
    uint16_t flat_transition_ms;
    uint8_t  dog_style;
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
    uint16_t flat_mute_threshold_lsb;
    uint16_t flat_transition_ms;
} vox_config_v6_wire_t;

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
} vox_config_v5_wire_t;

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
_Static_assert(sizeof(vox_config_v5_wire_t) == 31, "v5 config wire size changed");
_Static_assert(sizeof(vox_config_v6_wire_t) == 33, "v6 config wire size changed");
_Static_assert(sizeof(vox_config_wire_t) == 34, "v7 config wire size changed");
_Static_assert(offsetof(vox_config_wire_t, flat_mute_enabled) == 0,
               "v7 flat-mute offset changed");
_Static_assert(offsetof(vox_config_wire_t, btn_a_single) == 2,
               "v6 BtnA single offset changed");
_Static_assert(offsetof(vox_config_wire_t, btn_a_double) == 5,
               "v6 BtnA double offset changed");
_Static_assert(offsetof(vox_config_wire_t, btn_a_long) == 8,
               "v6 BtnA long offset changed");
_Static_assert(offsetof(vox_config_wire_t, btn_b_single) == 11,
               "v6 BtnB single offset changed");
_Static_assert(offsetof(vox_config_wire_t, btn_b_double) == 14,
               "v6 BtnB double offset changed");
_Static_assert(offsetof(vox_config_wire_t, btn_b_long) == 17,
               "v6 BtnB long offset changed");
_Static_assert(offsetof(vox_config_wire_t, shake) == 20,
               "v6 shake offset changed");
_Static_assert(offsetof(vox_config_wire_t, long_press_ms) == 23,
               "v6 long-press offset changed");
_Static_assert(offsetof(vox_config_wire_t, reserved2) == 25,
               "v6 reserved offset changed");
_Static_assert(offsetof(vox_config_wire_t, flat_mute_threshold_lsb) == 29,
               "v6 flat threshold offset changed");
_Static_assert(offsetof(vox_config_wire_t, flat_mute_threshold_lsb) ==
                   sizeof(vox_config_v4_wire_t),
               "v4 config must remain a v5 prefix");
_Static_assert(offsetof(vox_config_wire_t, flat_transition_ms) == 31,
               "v6 flat transition offset changed");
_Static_assert(offsetof(vox_config_wire_t, flat_transition_ms) ==
                   sizeof(vox_config_v5_wire_t),
               "v5 config must remain a v6 prefix");
_Static_assert(offsetof(vox_config_wire_t, dog_style) == 33,
               "v7 dog style offset changed");
_Static_assert(offsetof(vox_config_wire_t, dog_style) ==
                   sizeof(vox_config_v6_wire_t),
               "v6 config must remain a v7 prefix");

void vox_config_init(void);
void vox_config_get(vox_config_wire_t *out);
bool vox_config_flat_mute_enabled(void);
uint16_t vox_config_flat_mute_threshold_lsb(void);
uint8_t vox_config_dog_style(void);
esp_err_t vox_config_set(vox_config_wire_t const *cfg);
esp_err_t vox_config_reset(void);

#ifdef __cplusplus
}
#endif
