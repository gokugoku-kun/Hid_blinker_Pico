#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app_config.h"
#include "led_controller.h"
#include "led_protocol.h"

_Static_assert(APP_LED_COUNT >= 3 && APP_LED_COUNT < 255,
               "These tests need at least three independently controlled LEDs");

static unsigned assertions;
static unsigned test_cases;

#define CHECK(condition)                                                      \
    do {                                                                      \
        ++assertions;                                                         \
        if (!(condition)) {                                                   \
            fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__,  \
                    #condition);                                              \
            exit(EXIT_FAILURE);                                               \
        }                                                                     \
    } while (0)

#define RUN_TEST(function)           \
    do {                            \
        function();                 \
        ++test_cases;               \
        puts("PASS " #function);    \
    } while (0)

static void check_state(uint8_t id, led_mode_t mode, bool output_on,
                        uint32_t on_ms, uint32_t off_ms) {
    const led_state_t *state = led_controller_get(id);
    CHECK(state != NULL);
    CHECK(state->mode == mode);
    CHECK(state->output_on == output_on);
    CHECK(state->on_ms == on_ms);
    CHECK(state->off_ms == off_ms);
}

static void save_states(led_state_t saved[APP_LED_COUNT]) {
    for (uint8_t id = 0; id < APP_LED_COUNT; ++id) {
        saved[id] = *led_controller_get(id);
    }
}

static void check_unchanged(const led_state_t saved[APP_LED_COUNT]) {
    for (uint8_t id = 0; id < APP_LED_COUNT; ++id) {
        check_state(id, saved[id].mode, saved[id].output_on, saved[id].on_ms,
                    saved[id].off_ms);
        CHECK(led_controller_get(id)->started_us == saved[id].started_us);
    }
}

static void test_startup_and_independent_leds(void) {
    led_controller_init();
    for (uint8_t id = 0; id < APP_LED_COUNT; ++id) {
        check_state(id, LED_MODE_OFF, false, 0, 0);
    }
    CHECK(led_controller_get(APP_LED_COUNT) == NULL);

    CHECK(led_controller_set(0, LED_MODE_ON, 0, 0, 1000));
    CHECK(led_controller_set(1, LED_MODE_BLINK, 100, 900, 1000));
    CHECK(led_controller_set(2, LED_MODE_BLINK, 500, 500, 1000));
    led_controller_tick(101000);
    check_state(0, LED_MODE_ON, true, 0, 0);
    check_state(1, LED_MODE_BLINK, false, 100, 900);
    check_state(2, LED_MODE_BLINK, true, 500, 500);
    for (uint8_t id = 3; id < APP_LED_COUNT; ++id) {
        check_state(id, LED_MODE_OFF, false, 0, 0);
    }

    const led_state_t second_blink = *led_controller_get(2);
    CHECK(led_controller_set(1, LED_MODE_ON, 0, 0, 123456));
    check_state(2, second_blink.mode, second_blink.output_on,
                second_blink.on_ms, second_blink.off_ms);
    CHECK(led_controller_get(2)->started_us == second_blink.started_us);
}

static void test_blink_start_and_boundaries(void) {
    const uint64_t start = 1234567;
    led_controller_init();
    CHECK(led_controller_set(0, LED_MODE_BLINK, 100, 900, start));
    check_state(0, LED_MODE_BLINK, true, 100, 900);
    led_controller_tick(start + 99999);
    CHECK(led_controller_get(0)->output_on);
    led_controller_tick(start + 100000);
    CHECK(!led_controller_get(0)->output_on);
    led_controller_tick(start + 999999);
    CHECK(!led_controller_get(0)->output_on);
    led_controller_tick(start + 1000000);
    CHECK(led_controller_get(0)->output_on);
    led_controller_tick(start + 1100000);
    CHECK(!led_controller_get(0)->output_on);
}

static void test_reconfiguration_and_cancel(void) {
    led_controller_init();
    CHECK(led_controller_set(0, LED_MODE_BLINK, 100, 900, 0));
    led_controller_tick(250000);
    CHECK(!led_controller_get(0)->output_on);
    CHECK(led_controller_set(0, LED_MODE_BLINK, 200, 300, 250000));
    CHECK(led_controller_get(0)->output_on);
    led_controller_tick(449999);
    CHECK(led_controller_get(0)->output_on);
    led_controller_tick(450000);
    CHECK(!led_controller_get(0)->output_on);

    /* Repeating the same BLINK request also starts a new on phase. */
    CHECK(led_controller_set(0, LED_MODE_BLINK, 200, 300, 450001));
    CHECK(led_controller_get(0)->output_on);
    led_controller_tick(650000);
    CHECK(led_controller_get(0)->output_on);
    led_controller_tick(650001);
    CHECK(!led_controller_get(0)->output_on);

    CHECK(led_controller_set(0, LED_MODE_ON, UINT32_MAX, UINT32_MAX, 650002));
    led_controller_tick(UINT64_C(1000000000000));
    check_state(0, LED_MODE_ON, true, 0, 0);
    CHECK(led_controller_set(0, LED_MODE_BLINK, 100, 900,
                             UINT64_C(1000000000000)));
    CHECK(led_controller_set(0, LED_MODE_OFF, UINT32_MAX, UINT32_MAX,
                             UINT64_C(1000000000001)));
    led_controller_tick(UINT64_C(2000000000000));
    check_state(0, LED_MODE_OFF, false, 0, 0);
}

static void test_long_gap_and_counter_wrap(void) {
    const uint64_t start = 42;
    led_controller_init();
    CHECK(led_controller_set(0, LED_MODE_BLINK, 100, 900, start));
    /* One billion missed periods must not cause catch-up work or phase drift. */
    led_controller_tick(start + UINT64_C(1000000000000000) + 99999);
    CHECK(led_controller_get(0)->output_on);
    led_controller_tick(start + UINT64_C(1000000000000000) + 100000);
    CHECK(!led_controller_get(0)->output_on);
    led_controller_tick(start + UINT64_C(1000000000000000) + 1000000);
    CHECK(led_controller_get(0)->output_on);

    CHECK(led_controller_set(0, LED_MODE_BLINK, 500, 500,
                             UINT64_MAX - UINT64_C(499999)));
    led_controller_tick(UINT64_MAX);
    CHECK(led_controller_get(0)->output_on);
    led_controller_tick(0);
    CHECK(!led_controller_get(0)->output_on);
    led_controller_tick(499999);
    CHECK(!led_controller_get(0)->output_on);
    led_controller_tick(500000);
    CHECK(led_controller_get(0)->output_on);
}

static void test_invalid_controller_requests_are_atomic(void) {
    led_state_t saved[APP_LED_COUNT];
    led_controller_init();
    CHECK(led_controller_set(0, LED_MODE_BLINK, 100, 900, 123));
    CHECK(led_controller_set(1, LED_MODE_ON, 0, 0, 123));
    save_states(saved);

    CHECK(!led_controller_set(APP_LED_COUNT, LED_MODE_ON, 0, 0, 456));
    check_unchanged(saved);
    CHECK(!led_controller_set(0, (led_mode_t)255, 100, 900, 456));
    check_unchanged(saved);
    CHECK(!led_controller_set(0, LED_MODE_BLINK, 0, 900, 456));
    check_unchanged(saved);
    CHECK(!led_controller_set(0, LED_MODE_BLINK, 100, 0, 456));
    check_unchanged(saved);
    CHECK(!led_controller_set(0, LED_MODE_BLINK, APP_BLINK_MIN_MS - 1,
                              APP_BLINK_MIN_MS, 456));
    check_unchanged(saved);
    CHECK(!led_controller_set(0, LED_MODE_BLINK, APP_BLINK_MIN_MS,
                              APP_BLINK_MIN_MS - 1, 456));
    check_unchanged(saved);
    CHECK(!led_controller_set(0, LED_MODE_BLINK, APP_BLINK_MAX_MS + 1,
                              APP_BLINK_MAX_MS, 456));
    check_unchanged(saved);
    CHECK(!led_controller_set(0, LED_MODE_BLINK, APP_BLINK_MAX_MS,
                              APP_BLINK_MAX_MS + 1, 456));
    check_unchanged(saved);
    CHECK(!led_controller_set_all(LED_MODE_BLINK));
    check_unchanged(saved);
    CHECK(!led_controller_set_all((led_mode_t)255));
    check_unchanged(saved);

    CHECK(led_controller_set(0, LED_MODE_BLINK, APP_BLINK_MIN_MS,
                             APP_BLINK_MAX_MS, 456));
    check_state(0, LED_MODE_BLINK, true, APP_BLINK_MIN_MS, APP_BLINK_MAX_MS);
    CHECK(led_controller_set(0, LED_MODE_BLINK, APP_BLINK_MAX_MS,
                             APP_BLINK_MIN_MS, 456));
    check_state(0, LED_MODE_BLINK, true, APP_BLINK_MAX_MS, APP_BLINK_MIN_MS);
}

static void test_set_all_and_reinitialization(void) {
    led_controller_init();
    for (uint8_t id = 0; id < APP_LED_COUNT; ++id) {
        CHECK(led_controller_set(id, LED_MODE_BLINK, 100, 900, 456));
    }
    CHECK(led_controller_set_all(LED_MODE_ON));
    led_controller_tick(UINT64_C(1000000000000));
    for (uint8_t id = 0; id < APP_LED_COUNT; ++id) {
        check_state(id, LED_MODE_ON, true, 0, 0);
    }
    CHECK(led_controller_set_all(LED_MODE_OFF));
    led_controller_tick(UINT64_C(2000000000000));
    for (uint8_t id = 0; id < APP_LED_COUNT; ++id) {
        check_state(id, LED_MODE_OFF, false, 0, 0);
    }
    CHECK(led_controller_set_all(LED_MODE_ON));
    led_controller_init();
    for (uint8_t id = 0; id < APP_LED_COUNT; ++id) {
        check_state(id, LED_MODE_OFF, false, 0, 0);
    }
}

/* Build PC-side packets using the documented wire positions. Keeping these
 * positions literal makes the tests catch accidental firmware layout changes.
 */
static void request_init(uint8_t request[64], uint8_t command, uint8_t id,
                         uint8_t mode) {
    memset(request, 0, 64);
    request[0] = 1;
    request[1] = command;
    request[2] = 0x34;
    request[3] = 0x12;
    request[4] = id;
    request[5] = mode;
}

static void request_blink(uint8_t request[64]) {
    request_init(request, 1, 0, 2);
    request[8] = 100;             /* 100 ms on */
    request[12] = 0x84;           /* 900 ms off */
    request[13] = 0x03;
}

static void check_response_tail(const uint8_t response[64]) {
    for (size_t i = 25; i < 64; ++i) {
        CHECK(response[i] == 0);
    }
}

static void test_protocol_wire_roundtrip_and_aliasing(void) {
    /* Distinct byte values exercise little-endian 16- and 32-bit fields. */
    uint8_t request[64] = {
        1, 1, 0x34, 0x12, 2, 2, 0, 0,
        0x03, 0x02, 0x01, 0, 0x30, 0x20, 0x10, 0,
    };
    const uint8_t expected[64] = {
        1, 0x81, 0x34, 0x12, 0, 2, 2, 1,
        0x03, 0x02, 0x01, 0, 0x30, 0x20, 0x10, 0,
        0x04, 0x03, 0x02, 0x01, APP_LED_COUNT,
        APP_FW_VERSION_MAJOR, APP_FW_VERSION_MINOR, APP_FW_VERSION_PATCH, 1,
    };
    uint8_t original[64];
    uint8_t response[64];
    const uint64_t now = UINT64_C(0x01020304) * 1000 + 999;
    memcpy(original, request, sizeof(request));
    memset(response, 0xa5, sizeof(response));
    led_controller_init();
    led_protocol_handle(request, sizeof(request), now,
                        LED_RESET_WATCHDOG_TIMEOUT, response);
    CHECK(memcmp(response, expected, sizeof(expected)) == 0);
    CHECK(memcmp(request, original, sizeof(original)) == 0);
    check_state(2, LED_MODE_BLINK, true, 66051, 1056816);
    CHECK(led_controller_get(2)->started_us == now);
    check_state(0, LED_MODE_OFF, false, 0, 0);
    check_state(1, LED_MODE_OFF, false, 0, 0);

    led_controller_init();
    led_protocol_handle(request, sizeof(request), now,
                        LED_RESET_WATCHDOG_TIMEOUT, request);
    CHECK(memcmp(request, expected, sizeof(expected)) == 0);
    check_state(2, LED_MODE_BLINK, true, 66051, 1056816);
}

static void test_protocol_status_and_metadata(void) {
    uint8_t request[64];
    uint8_t response[64];
    led_state_t saved[APP_LED_COUNT];
    led_controller_init();
    CHECK(led_controller_set(1, LED_MODE_BLINK, 100, 900, 0));
    led_controller_tick(100000);
    save_states(saved);
    request_init(request, 2, 1, 0);
    for (uint8_t reset_reason = 0; reset_reason <= 2; ++reset_reason) {
        memset(response, 0xa5, sizeof(response));
        led_protocol_handle(request, sizeof(request), 100000, reset_reason,
                            response);
        CHECK(response[0] == 1);
        CHECK(response[1] == 0x82);
        CHECK(response[2] == 0x34 && response[3] == 0x12);
        CHECK(response[4] == 0);
        CHECK(response[5] == 1);
        CHECK(response[6] == 2 && response[7] == 0);
        CHECK(response[8] == 100 && response[9] == 0);
        CHECK(response[10] == 0 && response[11] == 0);
        CHECK(response[12] == 0x84 && response[13] == 3);
        CHECK(response[14] == 0 && response[15] == 0);
        CHECK(response[16] == 100 && response[17] == 0);
        CHECK(response[18] == 0 && response[19] == 0);
        CHECK(response[20] == APP_LED_COUNT);
        CHECK(response[21] == APP_FW_VERSION_MAJOR);
        CHECK(response[22] == APP_FW_VERSION_MINOR);
        CHECK(response[23] == APP_FW_VERSION_PATCH);
        CHECK(response[24] == reset_reason);
        check_response_tail(response);
        check_unchanged(saved);
    }

    const uint64_t wrap_time = (UINT64_C(0xffffffff) + 1) * 1000;
    led_protocol_handle(request, sizeof(request), wrap_time - 1, 0, response);
    CHECK(response[16] == 0xff && response[17] == 0xff);
    CHECK(response[18] == 0xff && response[19] == 0xff);
    led_protocol_handle(request, sizeof(request), wrap_time + 42000, 0,
                        response);
    CHECK(response[16] == 42 && response[17] == 0);
    CHECK(response[18] == 0 && response[19] == 0);
    check_unchanged(saved);
}

static void test_protocol_on_off_and_global_commands(void) {
    uint8_t request[64];
    uint8_t response[64];
    led_controller_init();
    request_blink(request);
    led_protocol_handle(request, sizeof(request), 0, 0, response);
    CHECK(response[4] == 0);
    led_controller_tick(100000);
    CHECK(!led_controller_get(0)->output_on);

    /* ON and OFF ignore supplied timing bytes and clear stored blink times. */
    request_init(request, 1, 0, 1);
    memset(request + 8, 0xff, 8);
    led_protocol_handle(request, sizeof(request), 100001, 0, response);
    CHECK(response[4] == 0);
    CHECK(response[6] == 1 && response[7] == 1);
    for (size_t i = 8; i < 16; ++i) { CHECK(response[i] == 0); }
    led_controller_tick(1000000);
    check_state(0, LED_MODE_ON, true, 0, 0);
    request[5] = 0;
    led_protocol_handle(request, sizeof(request), 1000001, 0, response);
    CHECK(response[4] == 0);
    CHECK(response[6] == 0 && response[7] == 0);
    check_state(0, LED_MODE_OFF, false, 0, 0);

    for (uint8_t id = 0; id < APP_LED_COUNT; ++id) {
        CHECK(led_controller_set(id, LED_MODE_BLINK, 100, 900, 2000000));
    }
    request_init(request, 4, 0xff, 1);
    led_protocol_handle(request, sizeof(request), 2000001, 0, response);
    CHECK(response[1] == 0x84 && response[4] == 0);
    CHECK(response[5] == 0xff);
    CHECK(response[6] == 1 && response[7] == 1);
    led_controller_tick(3000000);
    for (uint8_t id = 0; id < APP_LED_COUNT; ++id) {
        check_state(id, LED_MODE_ON, true, 0, 0);
    }
    request[5] = 0;
    led_protocol_handle(request, sizeof(request), 3000001, 0, response);
    CHECK(response[4] == 0);
    CHECK(response[6] == 0 && response[7] == 0);
    for (uint8_t id = 0; id < APP_LED_COUNT; ++id) {
        check_state(id, LED_MODE_OFF, false, 0, 0);
        CHECK(led_controller_set(id, LED_MODE_BLINK, 100, 900, 4000000));
    }

    request_init(request, 3, 0xff, 0);
    led_protocol_handle(request, sizeof(request), 4000001, 0, response);
    CHECK(response[1] == 0x83 && response[4] == 0);
    CHECK(response[5] == 0xff);
    CHECK(response[6] == 0 && response[7] == 0);
    led_controller_tick(5000000);
    for (uint8_t id = 0; id < APP_LED_COUNT; ++id) {
        check_state(id, LED_MODE_OFF, false, 0, 0);
    }
}

static void expect_rejected(const uint8_t request[64], uint8_t status) {
    uint8_t response[64];
    led_state_t saved[APP_LED_COUNT];
    save_states(saved);
    memset(response, 0xa5, sizeof(response));
    led_protocol_handle(request, 64, 987654, 2, response);
    CHECK(response[0] == 1);
    CHECK(response[1] == (uint8_t)(request[1] | 0x80));
    CHECK(response[2] == request[2] && response[3] == request[3]);
    CHECK(response[4] == status);
    CHECK(response[5] == request[4]);
    CHECK(response[20] == APP_LED_COUNT);
    CHECK(response[24] == 2);
    check_response_tail(response);
    if (request[4] < APP_LED_COUNT) {
        CHECK(response[6] == saved[request[4]].mode);
        CHECK(response[7] == saved[request[4]].output_on);
    } else {
        for (size_t i = 6; i < 16; ++i) { CHECK(response[i] == 0); }
    }
    check_unchanged(saved);
}

static void test_protocol_rejects_invalid_fields(void) {
    uint8_t request[64];
    led_controller_init();
    CHECK(led_controller_set(0, LED_MODE_BLINK, 100, 900, 0));
    CHECK(led_controller_set(1, LED_MODE_ON, 0, 0, 0));
    led_controller_tick(100000);

    const struct { uint8_t offset; uint8_t value; uint8_t status; } cases[] = {
        {0, 0, 2}, {0, 2, 2}, {0, 255, 2},
        {1, 0, 3}, {1, 5, 3}, {1, 0x81, 3}, {1, 255, 3},
        {4, APP_LED_COUNT, 4}, {4, 0xff, 4},
        {5, 3, 5}, {5, 255, 5},
        {8, 0, 6}, {8, APP_BLINK_MIN_MS - 1, 6},
        {10, 0xff, 6}, {11, 1, 6}, {14, 0xff, 6}, {15, 1, 6},
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        request_blink(request);
        request[cases[i].offset] = cases[i].value;
        expect_rejected(request, cases[i].status);
    }
    request_blink(request);
    memset(request + 12, 0, 4);
    expect_rejected(request, 6);

    for (size_t offset = 6; offset < 64; ++offset) {
        if (offset >= 8 && offset < 16) { continue; }
        request_blink(request);
        request[offset] = 1;
        expect_rejected(request, 7);
    }

    /* Unused fields in query/global commands are reserved, not blink times. */
    const uint8_t commands[] = {2, 3, 4};
    for (size_t c = 0; c < sizeof(commands); ++c) {
        const uint8_t command = commands[c];
        const uint8_t id = command == 2 ? 0 : 0xff;
        for (size_t offset = 8; offset < 16; ++offset) {
            request_init(request, command, id, 0);
            request[offset] = 1;
            expect_rejected(request, 7);
        }
        if (command != 4) {
            request_init(request, command, id, 1);
            expect_rejected(request, 7);
        }
        request_init(request, command, command == 2 ? 0xff : 0, 0);
        expect_rejected(request, 4);
    }
    request_init(request, 4, 0xff, 2);
    expect_rejected(request, 5);
    request[5] = 255;
    expect_rejected(request, 5);
}

static void test_protocol_lengths_and_response_bounds(void) {
    uint8_t request[65];
    led_state_t saved[APP_LED_COUNT];
    struct {
        uint8_t before[8];
        uint8_t report[64];
        uint8_t after[8];
    } guarded;
    led_controller_init();
    CHECK(led_controller_set(0, LED_MODE_BLINK, 100, 900, 0));
    save_states(saved);
    request_init(request, 1, 0, 1);
    request[64] = 0;

    for (size_t length = 0; length <= sizeof(request); ++length) {
        if (length == 64) { continue; }
        memset(&guarded, 0xa5, sizeof(guarded));
        led_protocol_handle(request, length, 123456, 0, guarded.report);
        CHECK(guarded.report[4] == 1);
        CHECK(guarded.report[1] == (length > 1 ? 0x81 : 0x80));
        CHECK(guarded.report[2] == (length > 2 ? 0x34 : 0));
        CHECK(guarded.report[3] == (length > 3 ? 0x12 : 0));
        CHECK(guarded.report[5] == (length > 4 ? 0 : 0xff));
        for (size_t i = 0; i < sizeof(guarded.before); ++i) {
            CHECK(guarded.before[i] == 0xa5);
            CHECK(guarded.after[i] == 0xa5);
        }
        check_response_tail(guarded.report);
        check_unchanged(saved);
    }

    led_protocol_handle(NULL, 64, 0, 0, guarded.report);
    CHECK(guarded.report[4] == 1);
    CHECK(guarded.report[1] == 0x80);
    CHECK(guarded.report[2] == 0 && guarded.report[3] == 0);
    CHECK(guarded.report[5] == 0xff);
    check_unchanged(saved);
}

int main(void) {
    RUN_TEST(test_startup_and_independent_leds);
    RUN_TEST(test_blink_start_and_boundaries);
    RUN_TEST(test_reconfiguration_and_cancel);
    RUN_TEST(test_long_gap_and_counter_wrap);
    RUN_TEST(test_invalid_controller_requests_are_atomic);
    RUN_TEST(test_set_all_and_reinitialization);
    RUN_TEST(test_protocol_wire_roundtrip_and_aliasing);
    RUN_TEST(test_protocol_status_and_metadata);
    RUN_TEST(test_protocol_on_off_and_global_commands);
    RUN_TEST(test_protocol_rejects_invalid_fields);
    RUN_TEST(test_protocol_lengths_and_response_bounds);
    printf("All %u host tests passed (%u assertions).\n", test_cases, assertions);
    return EXIT_SUCCESS;
}
