#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t vox_ble_hid_init(bool clear_bonds);
bool vox_ble_hid_ready(void);
uint32_t vox_ble_hid_generation(void);
esp_err_t vox_ble_hid_send_report(uint32_t generation,
								  uint8_t modifier, uint8_t keycode);

#ifdef __cplusplus
}
#endif