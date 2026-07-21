#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    VOX_HID_ROUTE_NONE = 0,
    VOX_HID_ROUTE_USB,
    VOX_HID_ROUTE_BLE,
} vox_hid_route_t;

typedef struct {
    vox_hid_route_t route;
    uint32_t generation;
} vox_hid_target_t;

esp_err_t vox_hid_transport_init(void);
vox_hid_target_t vox_hid_transport_select(void);
vox_hid_target_t vox_hid_transport_target(vox_hid_route_t route);
bool vox_hid_transport_ready(vox_hid_target_t target);
const char *vox_hid_transport_name(vox_hid_route_t route);
bool vox_hid_transport_send_tap(vox_hid_target_t target,
                                uint8_t modifier, uint8_t keycode);

#ifdef __cplusplus
}
#endif