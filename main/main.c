// scribestick firmware — Step 1: USB Audio Class mic only.
//
// Pipeline: ES8311 codec (I2C0 config + I2S RX data) → UAC input callback →
// host sees a 16 kHz / mono / 16-bit USB microphone called "StickS3-Mic".
//
// We rely on Espressif's `esp_codec_dev` to handle the ~100 lines of ES8311
// register dance (MCLK ratio, ADC gain, mic bias, etc.) and on
// `usb_device_uac` to handle every byte of the USB descriptor + class layer.
// The application code is intentionally thin so it stays auditable.
//
// Pin map (M5Stack StickS3):
//   I2C0 SDA=47   SCL=48                 -> ES8311 (addr 0x18) + PMIC + IMU
//   I2S0 MCLK=18  BCLK=17  WS=15  DIN=16 -> ES8311 ADC (mic data)
//
// PA / speaker output (DOUT=14) is intentionally untouched in v1; the StickS3
// PA defaults to ON via the PMIC and will tick if we leave I2S TX disabled,
// but until we drive the speaker that is acceptable. Step 3 will silence it
// via the M5PM1 register set.

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"
#include "driver/i2c_master.h"
#include "driver/i2s_std.h"
#include "usb_device_uac.h"
#include "esp_codec_dev.h"
#include "esp_codec_dev_defaults.h"

static const char *TAG = "scribestick";

// ---- Board pin map -------------------------------------------------------
#define I2C_PORT      I2C_NUM_0
#define I2C_SDA_PIN   47
#define I2C_SCL_PIN   48

#define I2S_PORT      I2S_NUM_0
#define I2S_MCLK_PIN  18
#define I2S_BCLK_PIN  17
#define I2S_WS_PIN    15
#define I2S_DIN_PIN   16

// ---- Audio format (must match sdkconfig CONFIG_UAC_*) --------------------
#define AUDIO_SAMPLE_RATE  16000
#define AUDIO_CHANNELS     1
#define AUDIO_BITS         16

// ---- Globals -------------------------------------------------------------
static i2c_master_bus_handle_t  g_i2c_bus     = NULL;
static i2s_chan_handle_t        g_i2s_rx      = NULL;
static esp_codec_dev_handle_t   g_codec_dev   = NULL;
static volatile bool            g_uac_muted   = false;

// =========================================================================
// I2C bus (shared with PMIC + IMU later, but for v1 only the codec uses it)
// =========================================================================
static esp_err_t i2c_bus_init(void)
{
    i2c_master_bus_config_t cfg = {
        .clk_source                  = I2C_CLK_SRC_DEFAULT,
        .i2c_port                    = I2C_PORT,
        .scl_io_num                  = I2C_SCL_PIN,
        .sda_io_num                  = I2C_SDA_PIN,
        .glitch_ignore_cnt           = 7,
        .flags.enable_internal_pullup = true,
    };
    return i2c_new_master_bus(&cfg, &g_i2c_bus);
}

// =========================================================================
// I2S RX channel — pure data path. Codec config goes through the I2C side.
// =========================================================================
static esp_err_t i2s_rx_init(void)
{
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_PORT, I2S_ROLE_MASTER);
    ESP_RETURN_ON_ERROR(i2s_new_channel(&chan_cfg, NULL, &g_i2s_rx),
                        TAG, "i2s_new_channel rx");

    i2s_std_config_t std_cfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(AUDIO_SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,
                                                       I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_MCLK_PIN,
            .bclk = I2S_BCLK_PIN,
            .ws   = I2S_WS_PIN,
            .dout = I2S_GPIO_UNUSED,
            .din  = I2S_DIN_PIN,
            .invert_flags = { 0 },
        },
    };
    // ES8311 wants MCLK = 256 * fs (256 * 16k = 4.096 MHz). The default
    // I2S_STD_CLK_DEFAULT_CONFIG sets mclk_multiple = 256, so this is fine.
    return i2s_channel_init_std_mode(g_i2s_rx, &std_cfg);
}

// =========================================================================
// ES8311 codec — register init via esp_codec_dev
// =========================================================================
static esp_err_t codec_init(void)
{
    audio_codec_i2c_cfg_t i2c_cfg = {
        .port    = I2C_PORT,
        .addr    = ES8311_CODEC_DEFAULT_ADDR,    // 7-bit addr 0x18 shifted
        .bus_handle = g_i2c_bus,
    };
    const audio_codec_ctrl_if_t *ctrl_if = audio_codec_new_i2c_ctrl(&i2c_cfg);
    ESP_RETURN_ON_FALSE(ctrl_if, ESP_FAIL, TAG, "i2c ctrl_if");

    audio_codec_i2s_cfg_t i2s_cfg = {
        .port      = I2S_PORT,
        .rx_handle = g_i2s_rx,
        .tx_handle = NULL,
    };
    const audio_codec_data_if_t *data_if = audio_codec_new_i2s_data(&i2s_cfg);
    ESP_RETURN_ON_FALSE(data_if, ESP_FAIL, TAG, "i2s data_if");

    es8311_codec_cfg_t es_cfg = {
        .ctrl_if    = ctrl_if,
        .gpio_if    = NULL,
        .codec_mode = ESP_CODEC_DEV_WORK_MODE_ADC,   // ADC = mic in only
        .pa_pin     = -1,
        .use_mclk   = true,
        .digital_mic = false,
    };
    const audio_codec_if_t *codec_if = es8311_codec_new(&es_cfg);
    ESP_RETURN_ON_FALSE(codec_if, ESP_FAIL, TAG, "es8311_codec_new");

    esp_codec_dev_cfg_t dev_cfg = {
        .codec_if = codec_if,
        .data_if  = data_if,
        .dev_type = ESP_CODEC_DEV_TYPE_IN,
    };
    g_codec_dev = esp_codec_dev_new(&dev_cfg);
    ESP_RETURN_ON_FALSE(g_codec_dev, ESP_FAIL, TAG, "esp_codec_dev_new");

    esp_codec_dev_sample_info_t fs = {
        .sample_rate     = AUDIO_SAMPLE_RATE,
        .channel         = AUDIO_CHANNELS,
        .bits_per_sample = AUDIO_BITS,
    };
    ESP_RETURN_ON_ERROR(esp_codec_dev_open(g_codec_dev, &fs),
                        TAG, "codec open");

    // Mic gain: ES8311 default ADC gain is mild. +30 dB makes a small MEMS
    // mic actually pick up speech at conversational distance. Tune later.
    esp_codec_dev_set_in_gain(g_codec_dev, 30.0);

    return ESP_OK;
}

// =========================================================================
// USB UAC callbacks — host pulls bytes from us via input_cb
// =========================================================================
static esp_err_t uac_input_cb(uint8_t *buf, size_t len, size_t *bytes_read, void *arg)
{
    if (!g_codec_dev) {
        return ESP_FAIL;
    }
    if (g_uac_muted) {
        memset(buf, 0, len);
        *bytes_read = len;
        return ESP_OK;
    }
    esp_err_t ret = esp_codec_dev_read(g_codec_dev, buf, len);
    *bytes_read = (ret == ESP_OK) ? len : 0;
    return ret;
}

static esp_err_t uac_output_cb(uint8_t *buf, size_t len, void *arg)
{
    // No speaker in v1 — host shouldn't even see an output stream because
    // CONFIG_UAC_MIC_*-only is configured, but be safe.
    return ESP_OK;
}

static void uac_set_mute_cb(uint32_t mute, void *arg)
{
    g_uac_muted = !!mute;
    ESP_LOGI(TAG, "host mute=%d", (int)mute);
}

static void uac_set_volume_cb(uint32_t volume, void *arg)
{
    // _volume = (volume_db + 50) * 2  -> volume_db
    int volume_db = (int)volume / 2 - 50;
    // Map volume_db to ES8311 in_gain (0..40 dB roughly). Below -10 dB just
    // ride at minimum; whisper.cpp won't transcribe useful audio quieter.
    float gain = 30.0f + (float)volume_db * 0.5f;
    if (gain < 0.0f)  gain = 0.0f;
    if (gain > 40.0f) gain = 40.0f;
    if (g_codec_dev) {
        esp_codec_dev_set_in_gain(g_codec_dev, gain);
    }
    ESP_LOGI(TAG, "host volume=%d dB -> mic gain=%.1f", volume_db, gain);
}

static esp_err_t uac_init(void)
{
    uac_device_config_t cfg = {
        .output_cb     = uac_output_cb,
        .input_cb      = uac_input_cb,
        .set_mute_cb   = uac_set_mute_cb,
        .set_volume_cb = uac_set_volume_cb,
        .cb_ctx        = NULL,
    };
    return uac_device_init(&cfg);
}

// =========================================================================
// app_main
// =========================================================================
void app_main(void)
{
    ESP_LOGI(TAG, "scribestick boot — fw v0.1");

    ESP_ERROR_CHECK(i2c_bus_init());
    ESP_LOGI(TAG, "i2c0 up (SDA=%d SCL=%d)", I2C_SDA_PIN, I2C_SCL_PIN);

    ESP_ERROR_CHECK(i2s_rx_init());
    ESP_LOGI(TAG, "i2s0 rx up (MCLK=%d BCLK=%d WS=%d DIN=%d)",
             I2S_MCLK_PIN, I2S_BCLK_PIN, I2S_WS_PIN, I2S_DIN_PIN);

    ESP_ERROR_CHECK(codec_init());
    ESP_LOGI(TAG, "es8311 ready @ %d Hz / %d ch / %d bit",
             AUDIO_SAMPLE_RATE, AUDIO_CHANNELS, AUDIO_BITS);

    ESP_ERROR_CHECK(uac_init());
    ESP_LOGI(TAG, "uac advertised as 'StickS3-Mic'");

    // Idle — UAC + I2S DMA do all the work in their own tasks.
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
