#ifndef APP_CONFIG_H
#define APP_CONFIG_H

/* LED 0 is the Pico W onboard LED; the remaining IDs follow this pin list. */
#define APP_EXTERNAL_LED_PINS {2u, 3u, 4u, 5u}
#define APP_EXTERNAL_LED_COUNT 4u
#define APP_LED_COUNT (1u + APP_EXTERNAL_LED_COUNT)
#define APP_BLINK_MIN_MS 10u
#define APP_BLINK_MAX_MS 3600000u

#define APP_WATCHDOG_INIT_MS 5000u
#define APP_WATCHDOG_RUN_MS 2000u

#define APP_PROTOCOL_VERSION 1u
#define APP_FW_VERSION_MAJOR 0u
#define APP_FW_VERSION_MINOR 2u
#define APP_FW_VERSION_PATCH 0u
#define APP_HID_REPORT_SIZE 64u
#define APP_HID_RX_QUEUE_DEPTH 4u

/* Shared PRIVATE TEST IDs, not globally unique. Do not distribute or sell
 * devices using these IDs. See https://pid.codes/1209/0001/ before reuse.
 */
#define APP_USB_VID 0x1209u
#define APP_USB_PID 0x0001u
#define APP_USB_USAGE_PAGE 0xFF00u
#define APP_USB_USAGE 0x0001u
#define APP_USB_MANUFACTURER "Personal Project"
#define APP_USB_PRODUCT "Hid_blinker_Pico"
#define APP_USB_SERIAL_PREFIX "PICOLED-"

#endif
