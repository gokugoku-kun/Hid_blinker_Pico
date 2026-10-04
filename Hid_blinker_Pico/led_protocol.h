#ifndef LED_PROTOCOL_H
#define LED_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

#include "app_config.h"

typedef enum {
    LED_CMD_SET_LED = 0x01,
    LED_CMD_GET_STATUS = 0x02,
    LED_CMD_ALL_OFF = 0x03,
    LED_CMD_SET_ALL = 0x04,
} led_protocol_command_t;

typedef enum {
    LED_STATUS_SUCCESS = 0,
    LED_STATUS_BAD_LENGTH = 1,
    LED_STATUS_BAD_VERSION = 2,
    LED_STATUS_BAD_COMMAND = 3,
    LED_STATUS_BAD_LED = 4,
    LED_STATUS_BAD_MODE = 5,
    LED_STATUS_BAD_TIMING = 6,
    LED_STATUS_BAD_RESERVED = 7,
} led_protocol_status_t;

typedef enum {
    LED_RESET_POWER_RUN_OTHER = 0,
    LED_RESET_WATCHDOG_TIMEOUT = 1,
    LED_RESET_OTHER_WATCHDOG = 2,
} led_protocol_reset_reason_t;

enum {
    LED_PROTOCOL_ALL_LEDS = 0xff,
    LED_PROTOCOL_RESPONSE_FLAG = 0x80,

    LED_REQUEST_VERSION = 0,
    LED_REQUEST_COMMAND = 1,
    LED_REQUEST_SEQUENCE = 2,  /* uint16_t, little-endian */
    LED_REQUEST_LED_ID = 4,
    LED_REQUEST_MODE = 5,
    LED_REQUEST_RESERVED = 6, /* bytes 6..7 must be zero */
    LED_REQUEST_ON_MS = 8,    /* uint32_t, little-endian */
    LED_REQUEST_OFF_MS = 12,  /* uint32_t, little-endian */
    LED_REQUEST_TAIL = 16,    /* bytes 16..63 must be zero */

    LED_RESPONSE_VERSION = 0,
    LED_RESPONSE_COMMAND = 1,
    LED_RESPONSE_SEQUENCE = 2,
    LED_RESPONSE_STATUS = 4,
    LED_RESPONSE_LED_ID = 5,
    LED_RESPONSE_MODE = 6,
    LED_RESPONSE_OUTPUT = 7,
    LED_RESPONSE_ON_MS = 8,
    LED_RESPONSE_OFF_MS = 12,
    LED_RESPONSE_UPTIME_MS = 16,
    LED_RESPONSE_LED_COUNT = 20,
    LED_RESPONSE_FW_MAJOR = 21,
    LED_RESPONSE_FW_MINOR = 22,
    LED_RESPONSE_FW_PATCH = 23,
    LED_RESPONSE_RESET_REASON = 24,
};

/* Interpret one 64-byte application payload, without a HID Report ID byte.
 * Initialize led_controller before calling this function. now_us shares the
 * controller's monotonic clock. No USB or physical GPIO operations occur here.
 *
 * SET_LED accepts modes OFF=0, ON=1, BLINK=2. OFF/ON ignore durations and store
 * zero. BLINK requires both durations within the configured inclusive limits.
 * GET_STATUS uses a single LED ID and zero mode/durations. ALL_OFF and SET_ALL
 * require ID=0xff and zero durations; ALL_OFF requires mode=0, while SET_ALL
 * accepts only OFF/ON. Other unused request bytes must be zero.
 *
 * Invalid requests leave all LED state unchanged. Errors for a valid LED ID
 * include its current state. Successful global commands echo their common
 * OFF/ON state; other responses without a valid individual ID have zero state.
 * Response bytes 25..63 are always zero. Uptime wraps after UINT32_MAX ms.
 *
 * response must point to APP_HID_REPORT_SIZE writable bytes. It may alias
 * request. A NULL request is treated as BAD_LENGTH; short requests echo only
 * the command/sequence/ID bytes that are present (missing ID is 0xff).
 */
void led_protocol_handle(const uint8_t *request, size_t request_len,
                         uint64_t now_us, uint8_t reset_reason,
                         uint8_t response[APP_HID_REPORT_SIZE]);

#endif
