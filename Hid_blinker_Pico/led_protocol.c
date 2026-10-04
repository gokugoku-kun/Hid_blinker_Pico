#include "led_protocol.h"

#include <stdbool.h>
#include <string.h>

#include "led_controller.h"

_Static_assert(APP_HID_REPORT_SIZE == 64, "Protocol v1 requires 64-byte reports");
_Static_assert(APP_LED_COUNT > 0 && APP_LED_COUNT < 256,
               "The 0xff LED ID is reserved for all LEDs");

static uint32_t read_u32_le(const uint8_t *bytes) {
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) |
           ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
}

static void write_u32_le(uint8_t *bytes, uint32_t value) {
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8);
    bytes[2] = (uint8_t)(value >> 16);
    bytes[3] = (uint8_t)(value >> 24);
}

static bool is_zero(const uint8_t *bytes, size_t length) {
    for (size_t i = 0; i < length; ++i) {
        if (bytes[i] != 0) {
            return false;
        }
    }
    return true;
}

static led_protocol_status_t execute_request(const uint8_t *request,
                                              size_t request_len,
                                              uint64_t now_us) {
    /* Validate completely before the first state-changing operation. */
    if (request == NULL || request_len != APP_HID_REPORT_SIZE) {
        return LED_STATUS_BAD_LENGTH;
    }
    if (request[LED_REQUEST_VERSION] != APP_PROTOCOL_VERSION) {
        return LED_STATUS_BAD_VERSION;
    }

    const uint8_t command = request[LED_REQUEST_COMMAND];
    if (command != LED_CMD_SET_LED && command != LED_CMD_GET_STATUS &&
        command != LED_CMD_ALL_OFF && command != LED_CMD_SET_ALL) {
        return LED_STATUS_BAD_COMMAND;
    }
    if (!is_zero(request + LED_REQUEST_RESERVED, 2) ||
        !is_zero(request + LED_REQUEST_TAIL,
                 APP_HID_REPORT_SIZE - LED_REQUEST_TAIL)) {
        return LED_STATUS_BAD_RESERVED;
    }

    const uint8_t id = request[LED_REQUEST_LED_ID];
    const uint8_t mode = request[LED_REQUEST_MODE];
    const uint32_t on_ms = read_u32_le(request + LED_REQUEST_ON_MS);
    const uint32_t off_ms = read_u32_le(request + LED_REQUEST_OFF_MS);

    if (command == LED_CMD_SET_LED || command == LED_CMD_GET_STATUS) {
        if (id >= APP_LED_COUNT) {
            return LED_STATUS_BAD_LED;
        }
    } else if (id != LED_PROTOCOL_ALL_LEDS) {
        return LED_STATUS_BAD_LED;
    }

    switch (command) {
        case LED_CMD_SET_LED:
            if (mode != LED_MODE_OFF && mode != LED_MODE_ON &&
                mode != LED_MODE_BLINK) {
                return LED_STATUS_BAD_MODE;
            }
            if (mode == LED_MODE_BLINK &&
                (on_ms < APP_BLINK_MIN_MS || on_ms > APP_BLINK_MAX_MS ||
                 off_ms < APP_BLINK_MIN_MS || off_ms > APP_BLINK_MAX_MS)) {
                return LED_STATUS_BAD_TIMING;
            }
            /* The checks above satisfy every controller precondition. */
            (void)led_controller_set(id, (led_mode_t)mode, on_ms, off_ms,
                                     now_us);
            break;

        case LED_CMD_GET_STATUS:
            if (mode != 0 || on_ms != 0 || off_ms != 0) {
                return LED_STATUS_BAD_RESERVED;
            }
            break;

        case LED_CMD_ALL_OFF:
            if (mode != 0 || on_ms != 0 || off_ms != 0) {
                return LED_STATUS_BAD_RESERVED;
            }
            (void)led_controller_set_all(LED_MODE_OFF);
            break;

        case LED_CMD_SET_ALL:
            if (mode != LED_MODE_OFF && mode != LED_MODE_ON) {
                return LED_STATUS_BAD_MODE;
            }
            if (on_ms != 0 || off_ms != 0) {
                return LED_STATUS_BAD_RESERVED;
            }
            (void)led_controller_set_all((led_mode_t)mode);
            break;

        default:
            return LED_STATUS_BAD_COMMAND;
    }

    return LED_STATUS_SUCCESS;
}

void led_protocol_handle(const uint8_t *request, size_t request_len,
                         uint64_t now_us, uint8_t reset_reason,
                         uint8_t response[APP_HID_REPORT_SIZE]) {
    const size_t available = request != NULL ? request_len : 0;
    const uint8_t command = available > LED_REQUEST_COMMAND
                                ? request[LED_REQUEST_COMMAND] : 0;
    const uint8_t sequence_lo = available > LED_REQUEST_SEQUENCE
                                    ? request[LED_REQUEST_SEQUENCE] : 0;
    const uint8_t sequence_hi = available > LED_REQUEST_SEQUENCE + 1
                                    ? request[LED_REQUEST_SEQUENCE + 1] : 0;
    const uint8_t id = available > LED_REQUEST_LED_ID
                           ? request[LED_REQUEST_LED_ID]
                           : LED_PROTOCOL_ALL_LEDS;
    const uint8_t mode = available > LED_REQUEST_MODE
                             ? request[LED_REQUEST_MODE] : 0;

    const led_protocol_status_t status =
        execute_request(request, request_len, now_us);

    /* All request reads precede the response write, allowing in-place use. */
    memset(response, 0, APP_HID_REPORT_SIZE);
    response[LED_RESPONSE_VERSION] = APP_PROTOCOL_VERSION;
    response[LED_RESPONSE_COMMAND] = command | LED_PROTOCOL_RESPONSE_FLAG;
    response[LED_RESPONSE_SEQUENCE] = sequence_lo;
    response[LED_RESPONSE_SEQUENCE + 1] = sequence_hi;
    response[LED_RESPONSE_STATUS] = (uint8_t)status;
    response[LED_RESPONSE_LED_ID] = id;

    const led_state_t *state = led_controller_get(id);
    if (state != NULL) {
        response[LED_RESPONSE_MODE] = (uint8_t)state->mode;
        response[LED_RESPONSE_OUTPUT] = state->output_on ? 1 : 0;
        write_u32_le(response + LED_RESPONSE_ON_MS, state->on_ms);
        write_u32_le(response + LED_RESPONSE_OFF_MS, state->off_ms);
    } else if (status == LED_STATUS_SUCCESS &&
               (command == LED_CMD_ALL_OFF || command == LED_CMD_SET_ALL)) {
        const uint8_t global_mode = command == LED_CMD_ALL_OFF
                                        ? LED_MODE_OFF : mode;
        response[LED_RESPONSE_MODE] = global_mode;
        response[LED_RESPONSE_OUTPUT] = global_mode == LED_MODE_ON ? 1 : 0;
    }

    write_u32_le(response + LED_RESPONSE_UPTIME_MS, (uint32_t)(now_us / 1000));
    response[LED_RESPONSE_LED_COUNT] = APP_LED_COUNT;
    response[LED_RESPONSE_FW_MAJOR] = APP_FW_VERSION_MAJOR;
    response[LED_RESPONSE_FW_MINOR] = APP_FW_VERSION_MINOR;
    response[LED_RESPONSE_FW_PATCH] = APP_FW_VERSION_PATCH;
    response[LED_RESPONSE_RESET_REASON] = reset_reason;
}
