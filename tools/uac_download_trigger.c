// Send voxstick's audio-class recovery sequence.
//
// The firmware recognizes an unlikely volume pattern on the UAC feature unit
// and reboots itself into ESP32-S3 ROM download mode. This path avoids macOS
// keyboard/HID permissions when the device is already enumerated as a mic.

#include <libusb.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#define VOX_VID 0x303a
#define VOX_PID 0x8001

#define UAC_AUDIO_CONTROL_ITF 0
#define UAC_FEATURE_UNIT_ENTITY 0x02
#define UAC_CS_REQ_CUR 0x01
#define UAC_FU_CTRL_MUTE 0x01
#define UAC_FU_CTRL_VOLUME 0x02

static int set_cur_index(libusb_device_handle *dev,
                         uint16_t index,
                         uint8_t control_selector,
                         uint8_t channel,
                         uint8_t *data,
                         uint16_t len)
{
    const uint8_t request_type =
        LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE;
    const uint16_t value = ((uint16_t)control_selector << 8) | channel;

    return libusb_control_transfer(dev, request_type, UAC_CS_REQ_CUR,
                                   value, index, data, len, 1000);
}

static int set_cur(libusb_device_handle *dev,
                   uint8_t control_selector,
                   uint8_t channel,
                   uint8_t *data,
                   uint16_t len)
{
    const uint16_t indexes[] = {
        ((uint16_t)UAC_FEATURE_UNIT_ENTITY << 8) | UAC_AUDIO_CONTROL_ITF,
        ((uint16_t)UAC_AUDIO_CONTROL_ITF << 8) | UAC_FEATURE_UNIT_ENTITY,
    };
    int last_rc = LIBUSB_ERROR_PIPE;

    for (size_t i = 0; i < sizeof(indexes) / sizeof(indexes[0]); i++) {
        last_rc = set_cur_index(dev, indexes[i], control_selector, channel,
                                data, len);
        if (last_rc >= 0) {
            return last_rc;
        }
    }

    return last_rc;
}

static int set_volume_db(libusb_device_handle *dev, uint8_t channel, int db)
{
    int16_t raw = (int16_t)(db * 256);
    uint8_t data[2] = {
        (uint8_t)(raw & 0xff),
        (uint8_t)(((uint16_t)raw >> 8) & 0xff),
    };
    return set_cur(dev, UAC_FU_CTRL_VOLUME, channel, data, sizeof(data));
}

static int set_mute(libusb_device_handle *dev, uint8_t channel, uint8_t mute)
{
    return set_cur(dev, UAC_FU_CTRL_MUTE, channel, &mute, sizeof(mute));
}

int main(void)
{
    int rc = libusb_init(NULL);
    if (rc != 0) {
        fprintf(stderr, "libusb_init failed: %s\n", libusb_error_name(rc));
        return 1;
    }

    libusb_device_handle *dev = libusb_open_device_with_vid_pid(NULL,
                                                                VOX_VID,
                                                                VOX_PID);
    if (!dev) {
        fprintf(stderr, "StickS3-Mic USB device not found\n");
        libusb_exit(NULL);
        return 1;
    }

    const uint8_t channels[] = { 0, 1 };
    int volume_pattern[] = { -49, -1, -49, -1 };
    for (size_t ch = 0; ch < sizeof(channels) / sizeof(channels[0]); ch++) {
        for (size_t i = 0; i < sizeof(volume_pattern) / sizeof(volume_pattern[0]); i++) {
            rc = set_volume_db(dev, channels[ch], volume_pattern[i]);
            if (rc < 0) {
                break;
            }
            usleep(150000);
        }

        if (rc >= 0) {
            printf("sent UAC volume download trigger to StickS3-Mic\n");
            libusb_close(dev);
            libusb_exit(NULL);
            return 0;
        }
    }

    uint8_t mute_pattern[] = { 1, 0, 1, 0, 1 };
    for (size_t ch = 0; ch < sizeof(channels) / sizeof(channels[0]); ch++) {
        for (size_t i = 0; i < sizeof(mute_pattern) / sizeof(mute_pattern[0]); i++) {
            rc = set_mute(dev, channels[ch], mute_pattern[i]);
            if (rc < 0) {
                break;
            }
            usleep(150000);
        }

        if (rc >= 0) {
            printf("sent UAC mute download trigger to StickS3-Mic\n");
            libusb_close(dev);
            libusb_exit(NULL);
            return 0;
        }
    }

    fprintf(stderr, "UAC SET_CUR failed: %s\n", libusb_error_name(rc));
    libusb_close(dev);
    libusb_exit(NULL);
    return 1;
}
