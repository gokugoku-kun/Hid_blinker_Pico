#include <stdio.h>
#include "hardware/watchdog.h"
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "tusb.h"

#include "app_config.h"
#include "led_controller.h"
#include "led_protocol.h"
#include "usb_transport.h"

static const uint external_pins[] = APP_EXTERNAL_LED_PINS;
static bool applied_outputs[APP_LED_COUNT];

_Static_assert(sizeof(external_pins) / sizeof(external_pins[0]) ==
                   APP_EXTERNAL_LED_COUNT, "LED pin list/count mismatch");
_Static_assert(APP_LED_COUNT < 256, "0xff is reserved for all LEDs");
_Static_assert(APP_WATCHDOG_INIT_MS <= 8388 && APP_WATCHDOG_RUN_MS <= 8388,
               "RP2040 watchdog limit exceeded");

static void external_leds_off(void) {
    for (size_t i = 0; i < APP_EXTERNAL_LED_COUNT; ++i) {
        gpio_put(external_pins[i], false);
    }
}

static void wait_for_watchdog(void) {
    external_leds_off();
    /* Do not feed from interrupts or error paths. */
    while (true) {
        tight_loop_contents();
    }
}

static bool apply_led_outputs(void) {
    for (uint8_t id = 0; id < APP_LED_COUNT; ++id) {
        const bool output = led_controller_get(id)->output_on;
        if (output == applied_outputs[id]) {
            continue;
        }
        if (id == 0) {
            /* The lower-level API reports CYW43 errors; the arch wrapper
             * discards them. Its internal lock is supplied by cyw43_arch.
             */
            if (cyw43_gpio_set(&cyw43_state, CYW43_WL_GPIO_LED_PIN, output)) {
                return false;
            }
        } else {
            gpio_put(external_pins[id - 1], output);
        }
        applied_outputs[id] = output;
    }
    return true;
}

/* TinyUSB's no-OS time source; no board_init() or USB stdio is needed. */
uint32_t tusb_time_millis_api(void) {
    return to_ms_since_boot(get_absolute_time());
}

int main(void)
{
    const uint8_t reset_reason = watchdog_enable_caused_reboot() ?
        LED_RESET_WATCHDOG_TIMEOUT :
        (watchdog_caused_reboot() ? LED_RESET_OTHER_WATCHDOG :
                                   LED_RESET_POWER_RUN_OTHER);

    for (size_t i = 0; i < APP_EXTERNAL_LED_COUNT; ++i) {
        gpio_init(external_pins[i]);
        gpio_put(external_pins[i], false);
        gpio_set_dir(external_pins[i], GPIO_OUT);
    }
    led_controller_init();
    watchdog_enable(APP_WATCHDOG_INIT_MS, false);
    stdio_init_all();

    if (cyw43_arch_init()) {
        puts("Failed to initialise CYW43");
        wait_for_watchdog();
    }
    if (cyw43_gpio_set(&cyw43_state, CYW43_WL_GPIO_LED_PIN, false)) {
        puts("Failed to turn off onboard LED");
        wait_for_watchdog();
    }

    usb_transport_init();
    const tusb_rhport_init_t usb_init = {
        .role = TUSB_ROLE_DEVICE,
        .speed = TUSB_SPEED_FULL,
    };
    if (!tusb_init(0, &usb_init)) {
        puts("Failed to initialise USB HID");
        wait_for_watchdog();
    }
    watchdog_enable(APP_WATCHDOG_RUN_MS, false);

    while (true) {
        uint8_t request[APP_HID_REPORT_SIZE];
        uint8_t response[APP_HID_REPORT_SIZE];
        size_t request_len;

        tud_task_ext(0, false);
        cyw43_arch_poll();
        const uint64_t now_us = time_us_64();
        led_controller_tick(now_us);

        const bool have_request = usb_transport_receive(request, &request_len);
        if (have_request) {
            led_protocol_handle(request, request_len, now_us, reset_reason,
                                response);
        }
        if (!apply_led_outputs()) {
            puts("Onboard LED driver error; waiting for watchdog");
            wait_for_watchdog();
        }
        if (have_request) {
            usb_transport_reply(response);
        }
        usb_transport_flush();

        /* Feed only after USB, command handling and LED outputs progressed.
         * No host heartbeat, connection test or receive deadline is involved.
         */
        watchdog_update();
        tight_loop_contents();
    }
}
