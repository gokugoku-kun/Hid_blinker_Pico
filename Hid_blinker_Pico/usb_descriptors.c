#include <string.h>

#include "pico/unique_id.h"
#include "tusb.h"
#include "app_config.h"

enum {
    STR_LANGUAGE = 0,
    STR_MANUFACTURER,
    STR_PRODUCT,
    STR_SERIAL,
};

static const tusb_desc_device_t device_descriptor = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = 0,
    .bDeviceSubClass = 0,
    .bDeviceProtocol = 0,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = APP_USB_VID,
    .idProduct = APP_USB_PID,
    .bcdDevice = 0x0020,
    .iManufacturer = STR_MANUFACTURER,
    .iProduct = STR_PRODUCT,
    .iSerialNumber = STR_SERIAL,
    .bNumConfigurations = 1,
};

/* Match the generic IN/OUT template, with configurable application usage. */
static const uint8_t report_descriptor[] = {
    HID_USAGE_PAGE_N(APP_USB_USAGE_PAGE, 2),
    HID_USAGE_N(APP_USB_USAGE, 2),
    HID_COLLECTION(HID_COLLECTION_APPLICATION),
        HID_USAGE(0x02),
        HID_LOGICAL_MIN(0),
        HID_LOGICAL_MAX_N(255, 2),
        HID_REPORT_SIZE(8),
        HID_REPORT_COUNT(APP_HID_REPORT_SIZE),
        HID_INPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE),
        HID_USAGE(0x03),
        HID_LOGICAL_MIN(0),
        HID_LOGICAL_MAX_N(255, 2),
        HID_REPORT_SIZE(8),
        HID_REPORT_COUNT(APP_HID_REPORT_SIZE),
        HID_OUTPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE),
    HID_COLLECTION_END
};

static const uint8_t configuration_descriptor[] = {
    TUD_CONFIG_DESCRIPTOR(1, 1, 0,
                          TUD_CONFIG_DESC_LEN + TUD_HID_INOUT_DESC_LEN,
                          0, 100),
    TUD_HID_INOUT_DESCRIPTOR(0, 0, HID_ITF_PROTOCOL_NONE,
                            sizeof(report_descriptor), 0x01, 0x81,
                            APP_HID_REPORT_SIZE, 10),
};

const uint8_t *tud_descriptor_device_cb(void) {
    return (const uint8_t *)&device_descriptor;
}

const uint8_t *tud_descriptor_configuration_cb(uint8_t index) {
    return index == 0 ? configuration_descriptor : NULL;
}

const uint8_t *tud_hid_descriptor_report_cb(uint8_t instance) {
    return instance == 0 ? report_descriptor : NULL;
}

const uint16_t *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    static uint16_t descriptor[33];
    static char serial[sizeof(APP_USB_SERIAL_PREFIX) +
                       2 * PICO_UNIQUE_BOARD_ID_SIZE_BYTES];
    const char *value;
    size_t length;

    if (index == STR_LANGUAGE) {
        descriptor[0] = (TUSB_DESC_STRING << 8) | 4;
        descriptor[1] = 0x0409;
        return descriptor;
    }
    if (langid != 0 && langid != 0x0409) {
        return NULL;
    }
    switch (index) {
        case STR_MANUFACTURER:
            value = APP_USB_MANUFACTURER;
            break;
        case STR_PRODUCT:
            value = APP_USB_PRODUCT;
            break;
        case STR_SERIAL:
            if (serial[0] == '\0') {
                memcpy(serial, APP_USB_SERIAL_PREFIX,
                       sizeof(APP_USB_SERIAL_PREFIX) - 1);
                pico_get_unique_board_id_string(
                    serial + sizeof(APP_USB_SERIAL_PREFIX) - 1,
                    2 * PICO_UNIQUE_BOARD_ID_SIZE_BYTES + 1);
            }
            value = serial;
            break;
        default:
            return NULL;
    }
    length = strlen(value);
    if (length > 32) {
        length = 32;
    }
    for (size_t i = 0; i < length; ++i) {
        descriptor[i + 1] = (uint8_t)value[i];
    }
    descriptor[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2 * length + 2));
    return descriptor;
}
