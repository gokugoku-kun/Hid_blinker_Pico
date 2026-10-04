#include "led_controller.h"

#include <stddef.h>

#include "app_config.h"

_Static_assert(APP_LED_COUNT > 0 && APP_LED_COUNT <= 256,
               "LED count must fit in the uint8_t ID range");
_Static_assert(APP_BLINK_MIN_MS > 0 &&
                   APP_BLINK_MIN_MS <= APP_BLINK_MAX_MS,
               "Blink limits must form a nonzero duration range");

static led_state_t led_states[APP_LED_COUNT];

void led_controller_init(void) {
    (void)led_controller_set_all(LED_MODE_OFF);
}

bool led_controller_set(uint8_t id, led_mode_t mode, uint32_t on_ms,
                        uint32_t off_ms, uint64_t now_us) {
    if (id >= APP_LED_COUNT) {
        return false;
    }

    if (mode != LED_MODE_OFF && mode != LED_MODE_ON &&
        mode != LED_MODE_BLINK) {
        return false;
    }

    if (mode == LED_MODE_BLINK &&
        (on_ms < APP_BLINK_MIN_MS || on_ms > APP_BLINK_MAX_MS ||
         off_ms < APP_BLINK_MIN_MS || off_ms > APP_BLINK_MAX_MS)) {
        return false;
    }

    led_states[id] = (led_state_t){
        .mode = mode,
        .output_on = mode != LED_MODE_OFF,
        .on_ms = mode == LED_MODE_BLINK ? on_ms : 0,
        .off_ms = mode == LED_MODE_BLINK ? off_ms : 0,
        .started_us = mode == LED_MODE_BLINK ? now_us : 0,
    };
    return true;
}

bool led_controller_set_all(led_mode_t mode) {
    if (mode != LED_MODE_OFF && mode != LED_MODE_ON) {
        return false;
    }

    for (size_t id = 0; id < APP_LED_COUNT; ++id) {
        led_states[id] = (led_state_t){
            .mode = mode,
            .output_on = mode == LED_MODE_ON,
            .on_ms = 0,
            .off_ms = 0,
            .started_us = 0,
        };
    }
    return true;
}

void led_controller_tick(uint64_t now_us) {
    for (size_t id = 0; id < APP_LED_COUNT; ++id) {
        led_state_t *state = &led_states[id];
        if (state->mode != LED_MODE_BLINK) {
            continue;
        }

        const uint64_t on_us = (uint64_t)state->on_ms * 1000;
        const uint64_t period_us = on_us + (uint64_t)state->off_ms * 1000;
        /* Derive the phase from the original start time. A delayed loop skips
         * missed edges without shifting the cycle or performing catch-up work.
         */
        const uint64_t phase_us = (now_us - state->started_us) % period_us;
        state->output_on = phase_us < on_us;
    }
}

const led_state_t *led_controller_get(uint8_t id) {
    return id < APP_LED_COUNT ? &led_states[id] : NULL;
}
