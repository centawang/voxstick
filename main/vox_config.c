#include "vox_config.h"

#include <stddef.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "nvs.h"

#define VOX_CONFIG_MAGIC        0x31474656u  // "VFG1", little-endian
#define VOX_CONFIG_STORE_VER    11
#define VOX_CONFIG_STORE_VER_MIN_LEGACY 1
#define VOX_CONFIG_STORE_VER_MAX_LEGACY 4
#define VOX_CONFIG_STORE_VER_V5 5
#define VOX_CONFIG_STORE_VER_V6 6
#define VOX_CONFIG_STORE_VER_V7 7
#define VOX_CONFIG_STORE_VER_V8 8
#define VOX_CONFIG_STORE_VER_V9 9
#define VOX_CONFIG_STORE_VER_V10 10
#define VOX_CONFIG_NAMESPACE    "voxstick"
#define VOX_CONFIG_KEY          "cfg"

#define HID_MOD_LEFT_CTRL       0x01
#define HID_KEY_ENTER           0x28
#define HID_KEY_BACKSPACE       0x2A
#define HID_KEY_ARROW_RIGHT     0x4F
#define HID_KEY_ARROW_LEFT      0x50
#define HID_KEY_ARROW_DOWN      0x51
#define HID_KEY_ARROW_UP        0x52

#define LONG_PRESS_MIN_MS       250
#define LONG_PRESS_MAX_MS       2000
#define ACTION_REPEAT_MIN       1
#define ACTION_REPEAT_MAX       100

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint16_t version;
    uint16_t size;
    vox_config_wire_t data;
} vox_config_store_t;

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint16_t version;
    uint16_t size;
    vox_config_v1_wire_t data;
} vox_config_legacy_store_t;

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint16_t version;
    uint16_t size;
    vox_config_v2_wire_t data;
} vox_config_v5_store_t;

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint16_t version;
    uint16_t size;
    vox_config_v3_wire_t data;
} vox_config_v6_store_t;

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint16_t version;
    uint16_t size;
    vox_config_v4_wire_t data;
} vox_config_v7_store_t;

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint16_t version;
    uint16_t size;
    vox_config_v5_wire_t data;
} vox_config_v8_store_t;

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint16_t version;
    uint16_t size;
    vox_config_v6_wire_t data;
} vox_config_v9_store_t;

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint16_t version;
    uint16_t size;
    vox_config_v7_wire_t data;
} vox_config_v10_store_t;

_Static_assert(sizeof(vox_config_legacy_store_t) == 20,
               "legacy config store size changed");
_Static_assert(sizeof(vox_config_v5_store_t) == 28,
               "v5 config store size changed");
_Static_assert(sizeof(vox_config_v6_store_t) == 34,
               "v6 config store size changed");
_Static_assert(sizeof(vox_config_v7_store_t) == 37,
               "v7 config store size changed");
_Static_assert(sizeof(vox_config_v8_store_t) == 39,
               "v8 config store size changed");
_Static_assert(sizeof(vox_config_v9_store_t) == 41,
               "v9 config store size changed");
_Static_assert(sizeof(vox_config_v10_store_t) == 42,
               "v10 config store size changed");
_Static_assert(sizeof(vox_config_store_t) == 43,
               "current config store size changed");

static const char *TAG = "voxstick";
static portMUX_TYPE s_config_lock = portMUX_INITIALIZER_UNLOCKED;
static vox_config_wire_t s_config;

static vox_config_wire_t default_config(void)
{
    return (vox_config_wire_t) {
        .flat_mute_enabled = 1,
        .reserved = 0,
        .btn_a_single = { .modifier = 0, .keycode = HID_KEY_ENTER, .repeat_count = 1 },
        .btn_a_double = { .modifier = HID_MOD_LEFT_CTRL, .keycode = 0, .repeat_count = 1 },
        .btn_a_long = { .modifier = 0, .keycode = HID_KEY_ARROW_RIGHT, .repeat_count = 1 },
        .btn_b_single = { .modifier = 0, .keycode = HID_KEY_ARROW_DOWN, .repeat_count = 1 },
        .btn_b_double = { .modifier = 0, .keycode = HID_KEY_ARROW_UP, .repeat_count = 1 },
        .btn_b_long = { .modifier = 0, .keycode = HID_KEY_ARROW_LEFT, .repeat_count = 1 },
        .shake = { .modifier = 0, .keycode = HID_KEY_BACKSPACE, .repeat_count = 20 },
        .long_press_ms = 600,
        .reserved2 = 0,
        .flat_mute_threshold_lsb = VOX_CONFIG_FLAT_THRESHOLD_DEFAULT_LSB,
        .flat_transition_ms = VOX_CONFIG_FLAT_TRANSITION_DEFAULT_MS,
        .dog_style = VOX_DOG_STYLE_DEFAULT,
        .completion_flash_count = VOX_CONFIG_COMPLETION_FLASH_DEFAULT,
    };
}

static bool action_valid(vox_hid_action_t action)
{
    if (action.repeat_count < ACTION_REPEAT_MIN ||
        action.repeat_count > ACTION_REPEAT_MAX) {
        return false;
    }
    if (action.repeat_count > 1 &&
        (action.modifier != 0 || action.keycode != HID_KEY_BACKSPACE)) {
        return false;
    }
    if (action.keycode == 0) {
        return true;
    }

    return action.keycode >= 0x04 && action.keycode <= 0x73;
}

static bool config_valid(vox_config_wire_t const *cfg)
{
    if (cfg == NULL) {
        return false;
    }
    if (!action_valid(cfg->btn_a_single) ||
        !action_valid(cfg->btn_a_double) ||
        !action_valid(cfg->btn_a_long) ||
        !action_valid(cfg->btn_b_single) ||
        !action_valid(cfg->btn_b_double) ||
        !action_valid(cfg->btn_b_long) ||
        !action_valid(cfg->shake)) {
        return false;
    }
    if (cfg->long_press_ms < LONG_PRESS_MIN_MS ||
        cfg->long_press_ms > LONG_PRESS_MAX_MS) {
        return false;
    }
    if (cfg->flat_mute_threshold_lsb < VOX_CONFIG_FLAT_THRESHOLD_MIN_LSB ||
        cfg->flat_mute_threshold_lsb > VOX_CONFIG_FLAT_THRESHOLD_MAX_LSB) {
        return false;
    }
    if (cfg->flat_transition_ms < VOX_CONFIG_FLAT_TRANSITION_MIN_MS ||
        cfg->flat_transition_ms > VOX_CONFIG_FLAT_TRANSITION_MAX_MS) {
        return false;
    }
    if (cfg->dog_style >= VOX_DOG_STYLE_COUNT) {
        return false;
    }
    if (cfg->completion_flash_count > VOX_CONFIG_COMPLETION_FLASH_MAX) {
        return false;
    }
    return true;
}

static esp_err_t config_save(vox_config_wire_t const *cfg)
{
    vox_config_store_t store = {
        .magic = VOX_CONFIG_MAGIC,
        .version = VOX_CONFIG_STORE_VER,
        .size = sizeof(vox_config_store_t),
        .data = *cfg,
    };

    nvs_handle_t nvs = 0;
    esp_err_t ret = nvs_open(VOX_CONFIG_NAMESPACE, NVS_READWRITE, &nvs);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = nvs_set_blob(nvs, VOX_CONFIG_KEY, &store, sizeof(store));
    if (ret == ESP_OK) {
        ret = nvs_commit(nvs);
    }
    nvs_close(nvs);
    return ret;
}

static esp_err_t config_load(vox_config_wire_t *out, bool *migrated)
{
    *migrated = false;
    nvs_handle_t nvs = 0;
    esp_err_t ret = nvs_open(VOX_CONFIG_NAMESPACE, NVS_READONLY, &nvs);
    if (ret != ESP_OK) {
        return ret;
    }

    size_t len = 0;
    ret = nvs_get_blob(nvs, VOX_CONFIG_KEY, NULL, &len);
    if (ret != ESP_OK) {
        nvs_close(nvs);
        return ret;
    }

    if (len == sizeof(vox_config_store_t)) {
        vox_config_store_t store = {0};
        size_t read_len = sizeof(store);
        ret = nvs_get_blob(nvs, VOX_CONFIG_KEY, &store, &read_len);
        nvs_close(nvs);
        if (ret != ESP_OK) {
            return ret;
        }
        if (read_len != sizeof(store) ||
            store.magic != VOX_CONFIG_MAGIC ||
            store.version != VOX_CONFIG_STORE_VER ||
            store.size != sizeof(store) ||
            !config_valid(&store.data)) {
            return ESP_ERR_INVALID_STATE;
        }
        *out = store.data;
        return ESP_OK;
    }

    if (len == sizeof(vox_config_v10_store_t)) {
        vox_config_v10_store_t store = {0};
        size_t read_len = sizeof(store);
        ret = nvs_get_blob(nvs, VOX_CONFIG_KEY, &store, &read_len);
        nvs_close(nvs);
        if (ret != ESP_OK) {
            return ret;
        }
        if (read_len != sizeof(store) ||
            store.magic != VOX_CONFIG_MAGIC ||
            store.version != VOX_CONFIG_STORE_VER_V10 ||
            store.size != sizeof(store)) {
            return ESP_ERR_INVALID_STATE;
        }

        vox_config_wire_t migrated_cfg = default_config();
        memcpy(&migrated_cfg, &store.data, sizeof(store.data));
        if (!config_valid(&migrated_cfg)) {
            return ESP_ERR_INVALID_STATE;
        }
        *out = migrated_cfg;
        *migrated = true;
        return ESP_OK;
    }

    if (len == sizeof(vox_config_v9_store_t)) {
        vox_config_v9_store_t store = {0};
        size_t read_len = sizeof(store);
        ret = nvs_get_blob(nvs, VOX_CONFIG_KEY, &store, &read_len);
        nvs_close(nvs);
        if (ret != ESP_OK) {
            return ret;
        }
        if (read_len != sizeof(store) ||
            store.magic != VOX_CONFIG_MAGIC ||
            store.version != VOX_CONFIG_STORE_VER_V9 ||
            store.size != sizeof(store)) {
            return ESP_ERR_INVALID_STATE;
        }

        vox_config_wire_t migrated_cfg = default_config();
        memcpy(&migrated_cfg, &store.data, sizeof(store.data));
        if (!config_valid(&migrated_cfg)) {
            return ESP_ERR_INVALID_STATE;
        }
        *out = migrated_cfg;
        *migrated = true;
        return ESP_OK;
    }

    if (len == sizeof(vox_config_v8_store_t)) {
        vox_config_v8_store_t store = {0};
        size_t read_len = sizeof(store);
        ret = nvs_get_blob(nvs, VOX_CONFIG_KEY, &store, &read_len);
        nvs_close(nvs);
        if (ret != ESP_OK) {
            return ret;
        }
        if (read_len != sizeof(store) ||
            store.magic != VOX_CONFIG_MAGIC ||
            store.version != VOX_CONFIG_STORE_VER_V8 ||
            store.size != sizeof(store)) {
            return ESP_ERR_INVALID_STATE;
        }

        vox_config_wire_t migrated_cfg = default_config();
        memcpy(&migrated_cfg, &store.data, sizeof(store.data));
        if (!config_valid(&migrated_cfg)) {
            return ESP_ERR_INVALID_STATE;
        }
        *out = migrated_cfg;
        *migrated = true;
        return ESP_OK;
    }

    if (len == sizeof(vox_config_v7_store_t)) {
        vox_config_v7_store_t store = {0};
        size_t read_len = sizeof(store);
        ret = nvs_get_blob(nvs, VOX_CONFIG_KEY, &store, &read_len);
        nvs_close(nvs);
        if (ret != ESP_OK) {
            return ret;
        }
        if (read_len != sizeof(store) ||
            store.magic != VOX_CONFIG_MAGIC ||
            store.version != VOX_CONFIG_STORE_VER_V7 ||
            store.size != sizeof(store)) {
            return ESP_ERR_INVALID_STATE;
        }

        vox_config_wire_t migrated_cfg = default_config();
        memcpy(&migrated_cfg, &store.data, sizeof(store.data));
        if (!config_valid(&migrated_cfg)) {
            return ESP_ERR_INVALID_STATE;
        }
        *out = migrated_cfg;
        *migrated = true;
        return ESP_OK;
    }

    if (len == sizeof(vox_config_v6_store_t)) {
        vox_config_v6_store_t store = {0};
        size_t read_len = sizeof(store);
        ret = nvs_get_blob(nvs, VOX_CONFIG_KEY, &store, &read_len);
        nvs_close(nvs);
        if (ret != ESP_OK) {
            return ret;
        }
        if (read_len != sizeof(store) ||
            store.magic != VOX_CONFIG_MAGIC ||
            store.version != VOX_CONFIG_STORE_VER_V6 ||
            store.size != sizeof(store)) {
            return ESP_ERR_INVALID_STATE;
        }

        vox_config_wire_t migrated_cfg = default_config();
        migrated_cfg.flat_mute_enabled = store.data.flat_mute_enabled;
        migrated_cfg.btn_a_single = store.data.btn_a_single;
        migrated_cfg.btn_a_double = store.data.btn_a_double;
        migrated_cfg.btn_a_long = store.data.btn_a_long;
        migrated_cfg.btn_b_single = store.data.btn_b_single;
        migrated_cfg.btn_b_double = store.data.btn_b_double;
        migrated_cfg.btn_b_long = store.data.btn_b_long;
        migrated_cfg.long_press_ms = store.data.long_press_ms;
        if (!config_valid(&migrated_cfg)) {
            return ESP_ERR_INVALID_STATE;
        }
        *out = migrated_cfg;
        *migrated = true;
        return ESP_OK;
    }

    if (len == sizeof(vox_config_v5_store_t)) {
        vox_config_v5_store_t store = {0};
        size_t read_len = sizeof(store);
        ret = nvs_get_blob(nvs, VOX_CONFIG_KEY, &store, &read_len);
        nvs_close(nvs);
        if (ret != ESP_OK) {
            return ret;
        }
        if (read_len != sizeof(store) ||
            store.magic != VOX_CONFIG_MAGIC ||
            store.version != VOX_CONFIG_STORE_VER_V5 ||
            store.size != sizeof(store)) {
            return ESP_ERR_INVALID_STATE;
        }

        vox_config_wire_t migrated_cfg = default_config();
        migrated_cfg.flat_mute_enabled = store.data.flat_mute_enabled;
        migrated_cfg.btn_a_single.modifier = store.data.btn_a_single.modifier;
        migrated_cfg.btn_a_single.keycode = store.data.btn_a_single.keycode;
        migrated_cfg.btn_a_double.modifier = store.data.btn_a_double.modifier;
        migrated_cfg.btn_a_double.keycode = store.data.btn_a_double.keycode;
        migrated_cfg.btn_a_long.modifier = store.data.btn_a_long.modifier;
        migrated_cfg.btn_a_long.keycode = store.data.btn_a_long.keycode;
        migrated_cfg.btn_b_single.modifier = store.data.btn_b_single.modifier;
        migrated_cfg.btn_b_single.keycode = store.data.btn_b_single.keycode;
        migrated_cfg.btn_b_double.modifier = store.data.btn_b_double.modifier;
        migrated_cfg.btn_b_double.keycode = store.data.btn_b_double.keycode;
        migrated_cfg.btn_b_long.modifier = store.data.btn_b_long.modifier;
        migrated_cfg.btn_b_long.keycode = store.data.btn_b_long.keycode;
        migrated_cfg.long_press_ms = store.data.long_press_ms;
        if (!config_valid(&migrated_cfg)) {
            return ESP_ERR_INVALID_STATE;
        }
        *out = migrated_cfg;
        *migrated = true;
        return ESP_OK;
    }

    if (len == sizeof(vox_config_legacy_store_t)) {
        vox_config_legacy_store_t legacy = {0};
        size_t read_len = sizeof(legacy);
        ret = nvs_get_blob(nvs, VOX_CONFIG_KEY, &legacy, &read_len);
        nvs_close(nvs);
        if (ret != ESP_OK) {
            return ret;
        }
        if (read_len != sizeof(legacy) ||
            legacy.magic != VOX_CONFIG_MAGIC ||
            legacy.version < VOX_CONFIG_STORE_VER_MIN_LEGACY ||
            legacy.version > VOX_CONFIG_STORE_VER_MAX_LEGACY ||
            legacy.size != sizeof(legacy)) {
            return ESP_ERR_INVALID_STATE;
        }

        vox_config_wire_t migrated_cfg = default_config();
        migrated_cfg.flat_mute_enabled = legacy.data.flat_mute_enabled;
        migrated_cfg.btn_a_single.modifier = legacy.data.tap_modifier;
        migrated_cfg.btn_a_single.keycode = legacy.data.tap_keycode;
        migrated_cfg.btn_a_long.modifier = legacy.data.hold_modifier;
        migrated_cfg.btn_a_long.keycode = legacy.data.hold_keycode;
        migrated_cfg.long_press_ms = legacy.data.long_press_ms;
        if (!config_valid(&migrated_cfg)) {
            return ESP_ERR_INVALID_STATE;
        }

        *out = migrated_cfg;
        *migrated = true;
        return ESP_OK;
    }

    nvs_close(nvs);
    return ESP_ERR_INVALID_STATE;
}

static void log_action(const char *name, vox_hid_action_t action)
{
    ESP_LOGI(TAG, "config: %-12s = 0x%02x+0x%02x x%u",
             name, action.modifier, action.keycode, action.repeat_count);
}

void vox_config_init(void)
{
    vox_config_wire_t cfg = default_config();
    bool migrated = false;
    esp_err_t ret = config_load(&cfg, &migrated);
    if (ret == ESP_ERR_NVS_NOT_FOUND || ret == ESP_ERR_NVS_NOT_INITIALIZED) {
        ESP_LOGI(TAG, "config: using defaults");
    } else if (ret != ESP_OK) {
        ESP_LOGW(TAG, "config load failed (%s); using defaults",
                 esp_err_to_name(ret));
    } else if (migrated) {
        esp_err_t save_ret = config_save(&cfg);
        if (save_ret == ESP_OK) {
            ESP_LOGI(TAG, "config migrated to seven-action store v%u",
                     VOX_CONFIG_STORE_VER);
        } else {
            ESP_LOGW(TAG, "config migration save failed: %s",
                     esp_err_to_name(save_ret));
        }
    }

    portENTER_CRITICAL(&s_config_lock);
    s_config = cfg;
    portEXIT_CRITICAL(&s_config_lock);

    ESP_LOGI(TAG, "config: flat_mute=%u threshold=%u lsb transition=%u ms long=%u ms dog=%u completion_flashes=%u",
             cfg.flat_mute_enabled, cfg.flat_mute_threshold_lsb,
             cfg.flat_transition_ms, cfg.long_press_ms, cfg.dog_style,
             cfg.completion_flash_count);
    log_action("BtnA single", cfg.btn_a_single);
    log_action("BtnA double", cfg.btn_a_double);
    log_action("BtnA long", cfg.btn_a_long);
    log_action("BtnB single", cfg.btn_b_single);
    log_action("BtnB double", cfg.btn_b_double);
    log_action("BtnB long", cfg.btn_b_long);
    log_action("Shake", cfg.shake);
}

void vox_config_get(vox_config_wire_t *out)
{
    if (out == NULL) {
        return;
    }
    portENTER_CRITICAL(&s_config_lock);
    *out = s_config;
    portEXIT_CRITICAL(&s_config_lock);
}

bool vox_config_flat_mute_enabled(void)
{
    bool enabled;
    portENTER_CRITICAL(&s_config_lock);
    enabled = s_config.flat_mute_enabled != 0;
    portEXIT_CRITICAL(&s_config_lock);
    return enabled;
}

uint16_t vox_config_flat_mute_threshold_lsb(void)
{
    uint16_t threshold;
    portENTER_CRITICAL(&s_config_lock);
    threshold = s_config.flat_mute_threshold_lsb;
    portEXIT_CRITICAL(&s_config_lock);
    return threshold;
}

uint8_t vox_config_dog_style(void)
{
    uint8_t dog_style;
    portENTER_CRITICAL(&s_config_lock);
    dog_style = s_config.dog_style;
    portEXIT_CRITICAL(&s_config_lock);
    return dog_style;
}

uint8_t vox_config_completion_flash_count(void)
{
    uint8_t count;
    portENTER_CRITICAL(&s_config_lock);
    count = s_config.completion_flash_count;
    portEXIT_CRITICAL(&s_config_lock);
    return count;
}

esp_err_t vox_config_set(vox_config_wire_t const *cfg)
{
    if (cfg == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    vox_config_wire_t normalized = *cfg;
    normalized.flat_mute_enabled = normalized.flat_mute_enabled ? 1 : 0;
    normalized.reserved = 0;
    normalized.reserved2 = 0;
    if (!config_valid(&normalized)) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = config_save(&normalized);
    if (ret != ESP_OK) {
        return ret;
    }

    portENTER_CRITICAL(&s_config_lock);
    s_config = normalized;
    portEXIT_CRITICAL(&s_config_lock);

    ESP_LOGI(TAG, "config saved: flat_mute=%u threshold=%u lsb transition=%u ms long=%u ms dog=%u completion_flashes=%u",
             normalized.flat_mute_enabled,
             normalized.flat_mute_threshold_lsb,
             normalized.flat_transition_ms,
             normalized.long_press_ms,
             normalized.dog_style,
             normalized.completion_flash_count);
    log_action("BtnA single", normalized.btn_a_single);
    log_action("BtnA double", normalized.btn_a_double);
    log_action("BtnA long", normalized.btn_a_long);
    log_action("BtnB single", normalized.btn_b_single);
    log_action("BtnB double", normalized.btn_b_double);
    log_action("BtnB long", normalized.btn_b_long);
    log_action("Shake", normalized.shake);
    return ESP_OK;
}

esp_err_t vox_config_reset(void)
{
    vox_config_wire_t cfg = default_config();
    return vox_config_set(&cfg);
}
