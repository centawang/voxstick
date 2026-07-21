#include <libusb.h>
#include <stdint.h>
#include <stdio.h>

#define VOX_VID 0x303a
#define VOX_PID 0x8001
#define VOX_NOTIFY_REQUEST 0x5e
#define VOX_NOTIFY_VALUE 0x5043
#define VOX_NOTIFY_INDEX 0x454e
#define VOX_TRANSFER_TIMEOUT_MS 500

static int report_error(const char *operation, int result)
{
    fprintf(stderr, "%s: %s\n", operation, libusb_error_name(result));
    return result == LIBUSB_SUCCESS ? 1 : -result;
}

int main(void)
{
    libusb_context *context = NULL;
    libusb_device_handle *handle = NULL;
    int result = libusb_init(&context);
    if (result != LIBUSB_SUCCESS) {
        return report_error("libusb init failed", result);
    }

    handle = libusb_open_device_with_vid_pid(context, VOX_VID, VOX_PID);
    if (handle == NULL) {
        fprintf(stderr, "vibestick USB device not found\n");
        libusb_exit(context);
        return 2;
    }

    const uint8_t request_type = LIBUSB_ENDPOINT_OUT |
        LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE;
    result = libusb_control_transfer(handle, request_type,
                                     VOX_NOTIFY_REQUEST,
                                     VOX_NOTIFY_VALUE,
                                     VOX_NOTIFY_INDEX,
                                     NULL, 0,
                                     VOX_TRANSFER_TIMEOUT_MS);
    libusb_close(handle);
    libusb_exit(context);
    if (result < 0) {
        return report_error("completion notification failed", result);
    }
    if (result != 0) {
        fprintf(stderr, "unexpected completion notification response length\n");
        return 1;
    }
    return 0;
}
