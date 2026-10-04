#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tusb.h"
#include "usb_transport.h"

static unsigned checks;
static bool host_ready;
static bool host_accepts_report;
static unsigned ready_calls;
static unsigned report_calls;
static uint8_t sent_id;
static uint16_t sent_length;
static uint8_t sent_report[APP_HID_REPORT_SIZE];

static void check(bool condition, const char *expression, int line) {
    ++checks;
    if (!condition) {
        fprintf(stderr, "FAIL line %d: %s\n", line, expression);
        exit(EXIT_FAILURE);
    }
}

#define CHECK(condition) check((condition), #condition, __LINE__)

bool tud_hid_ready(void) {
    ++ready_calls;
    return host_ready;
}

bool tud_hid_report(uint8_t report_id, const void *report, uint16_t length) {
    ++report_calls;
    CHECK(length <= sizeof(sent_report));
    sent_id = report_id;
    sent_length = length;
    memcpy(sent_report, report, length);
    return host_accepts_report;
}

static void reset_fixture(void) {
    host_ready = false;
    host_accepts_report = true;
    ready_calls = 0;
    report_calls = 0;
    sent_id = 0xff;
    sent_length = 0;
    memset(sent_report, 0, sizeof(sent_report));
    usb_transport_init();
}

static void enqueue(uint8_t marker) {
    uint8_t report[APP_HID_REPORT_SIZE];
    memset(report, marker, sizeof(report));
    tud_hid_set_report_cb(0, 0, HID_REPORT_TYPE_OUTPUT,
                          report, sizeof(report));
    /* The callback must keep its own copy, independent of the USB buffer. */
    memset(report, 0xee, sizeof(report));
}

static void receive_marker(uint8_t marker) {
    uint8_t request[APP_HID_REPORT_SIZE];
    uint8_t expected[APP_HID_REPORT_SIZE];
    size_t length = 0;
    memset(expected, marker, sizeof(expected));
    CHECK(usb_transport_receive(request, &length));
    CHECK(length == APP_HID_REPORT_SIZE);
    CHECK(memcmp(request, expected, sizeof(request)) == 0);
}

static void check_receive_empty(void) {
    uint8_t request[APP_HID_REPORT_SIZE];
    uint8_t expected[APP_HID_REPORT_SIZE];
    size_t length = 123;
    memset(request, 0xa7, sizeof(request));
    memset(expected, 0xa7, sizeof(expected));
    CHECK(!usb_transport_receive(request, &length));
    CHECK(length == 123);
    CHECK(memcmp(request, expected, sizeof(request)) == 0);
}

static void test_initial_state(void) {
    reset_fixture();
    check_receive_empty();
    usb_transport_flush();
    CHECK(ready_calls == 0);
    CHECK(report_calls == 0);
    uint8_t report[APP_HID_REPORT_SIZE] = {0};
    CHECK(tud_hid_get_report_cb(0, 0, HID_REPORT_TYPE_INPUT,
                                report, sizeof(report)) == 0);
}

static void test_fifo_overflow_and_wrap(void) {
    reset_fixture();
    for (unsigned i = 0; i < APP_HID_RX_QUEUE_DEPTH; ++i) {
        enqueue((uint8_t)(i + 1));
    }
    enqueue(0xf0); /* A full queue drops only this new request. */
    receive_marker(1);
    enqueue(0x80); /* Wrap the tail into the slot just released. */
    enqueue(0xf1); /* Still full: this request must also be discarded. */
    for (unsigned i = 1; i < APP_HID_RX_QUEUE_DEPTH; ++i) {
        receive_marker((uint8_t)(i + 1));
    }
    receive_marker(0x80);
    check_receive_empty();
    enqueue(0x81);
    receive_marker(0x81);
    check_receive_empty();
}

static void test_busy_tx_and_reply_order(void) {
    reset_fixture();
    enqueue(0x11);
    enqueue(0x22);
    receive_marker(0x11);
    uint8_t response[APP_HID_REPORT_SIZE];
    uint8_t expected[APP_HID_REPORT_SIZE];
    memset(response, 0xa1, sizeof(response));
    memset(expected, 0xa1, sizeof(expected));
    usb_transport_reply(response);
    memset(response, 0xee, sizeof(response));

    /* Every call must return even while the host is not taking reports. */
    for (unsigned i = 0; i < 10000; ++i) {
        usb_transport_flush();
    }
    CHECK(ready_calls == 10000);
    CHECK(report_calls == 0);
    check_receive_empty();

    host_ready = true;
    host_accepts_report = false;
    usb_transport_flush();
    CHECK(report_calls == 1);
    CHECK(sent_id == 0);
    CHECK(sent_length == APP_HID_REPORT_SIZE);
    CHECK(memcmp(sent_report, expected, sizeof(expected)) == 0);
    check_receive_empty();

    host_accepts_report = true;
    usb_transport_flush();
    CHECK(report_calls == 2);
    CHECK(memcmp(sent_report, expected, sizeof(expected)) == 0);
    receive_marker(0x22);
    usb_transport_flush();
    CHECK(report_calls == 2); /* No duplicate transmission once accepted. */

    memset(response, 0xa2, sizeof(response));
    usb_transport_reply(response);
    usb_transport_flush();
    CHECK(report_calls == 3);
    CHECK(memcmp(sent_report, response, sizeof(response)) == 0);
    check_receive_empty();
}

static void test_report_lengths(void) {
    reset_fixture();
    const uint8_t short_report[] = {1, 2, 3, 4, 5};
    uint8_t request[APP_HID_REPORT_SIZE];
    uint8_t expected[APP_HID_REPORT_SIZE] = {1, 2, 3, 4, 5};
    size_t length = 0;
    tud_hid_set_report_cb(0, 0, HID_REPORT_TYPE_OUTPUT,
                          short_report, sizeof(short_report));
    CHECK(usb_transport_receive(request, &length));
    CHECK(length == sizeof(short_report));
    CHECK(memcmp(request, expected, sizeof(request)) == 0);

    tud_hid_set_report_cb(0, 0, HID_REPORT_TYPE_OUTPUT, NULL, 0);
    CHECK(usb_transport_receive(request, &length));
    CHECK(length == 0);
    memset(expected, 0, sizeof(expected));
    CHECK(memcmp(request, expected, sizeof(request)) == 0);

    uint8_t large_report[APP_HID_REPORT_SIZE + 4];
    for (size_t i = 0; i < sizeof(large_report); ++i) {
        large_report[i] = (uint8_t)i;
    }
    tud_hid_set_report_cb(0, 0, HID_REPORT_TYPE_OUTPUT,
                          large_report, sizeof(large_report));
    CHECK(usb_transport_receive(request, &length));
    CHECK(length == sizeof(large_report)); /* Parser can reject oversize. */
    CHECK(memcmp(request, large_report, sizeof(request)) == 0);
    check_receive_empty();
}

static void test_unsupported_reports(void) {
    reset_fixture();
    uint8_t report[APP_HID_REPORT_SIZE];
    memset(report, 0x55, sizeof(report));
    tud_hid_set_report_cb(1, 0, HID_REPORT_TYPE_OUTPUT, report, sizeof(report));
    tud_hid_set_report_cb(0, 1, HID_REPORT_TYPE_OUTPUT, report, sizeof(report));
    tud_hid_set_report_cb(0, 0, HID_REPORT_TYPE_INPUT, report, sizeof(report));
    tud_hid_set_report_cb(0, 0, HID_REPORT_TYPE_FEATURE, report, sizeof(report));
    check_receive_empty();

    /* TinyUSB uses INVALID for the untyped interrupt-OUT path. */
    tud_hid_set_report_cb(0, 0, HID_REPORT_TYPE_INVALID,
                          report, sizeof(report));
    receive_marker(0x55);
    enqueue(0x66);
    receive_marker(0x66);
    check_receive_empty();
}

static void test_get_report(void) {
    reset_fixture();
    uint8_t response[APP_HID_REPORT_SIZE];
    uint8_t readback[APP_HID_REPORT_SIZE + 4];
    for (size_t i = 0; i < sizeof(response); ++i) {
        response[i] = (uint8_t)(0x40 + i);
    }
    usb_transport_reply(response);
    memset(readback, 0xab, sizeof(readback));
    CHECK(tud_hid_get_report_cb(1, 0, HID_REPORT_TYPE_INPUT,
                                readback, sizeof(readback)) == 0);
    CHECK(tud_hid_get_report_cb(0, 1, HID_REPORT_TYPE_INPUT,
                                readback, sizeof(readback)) == 0);
    CHECK(tud_hid_get_report_cb(0, 0, HID_REPORT_TYPE_OUTPUT,
                                readback, sizeof(readback)) == 0);
    CHECK(tud_hid_get_report_cb(0, 0, HID_REPORT_TYPE_FEATURE,
                                readback, sizeof(readback)) == 0);
    CHECK(tud_hid_get_report_cb(0, 0, HID_REPORT_TYPE_INPUT,
                                readback, 10) == 10);
    CHECK(memcmp(readback, response, 10) == 0);
    CHECK(readback[10] == 0xab);
    CHECK(tud_hid_get_report_cb(0, 0, HID_REPORT_TYPE_INPUT,
                                readback, sizeof(readback)) == APP_HID_REPORT_SIZE);
    CHECK(memcmp(readback, response, sizeof(response)) == 0);
    CHECK(readback[APP_HID_REPORT_SIZE] == 0xab);

    /* GET_REPORT observes the last reply without consuming pending TX. */
    host_ready = true;
    usb_transport_flush();
    CHECK(report_calls == 1);
    CHECK(tud_hid_get_report_cb(0, 0, HID_REPORT_TYPE_INPUT,
                                readback, sizeof(readback)) == APP_HID_REPORT_SIZE);
    CHECK(memcmp(readback, response, sizeof(response)) == 0);
}

static void test_usb_events(void) {
    for (unsigned event = 0; event < 5; ++event) {
        reset_fixture();
        enqueue(0x71);
        const uint8_t response[APP_HID_REPORT_SIZE] = {0x72};
        usb_transport_reply(response);
        switch (event) {
            case 0: tud_mount_cb(); break;
            case 1: tud_umount_cb(); break;
            case 2: tud_suspend_cb(false); break;
            case 3: tud_suspend_cb(true); break;
            default: usb_transport_init(); break;
        }
        host_ready = true;
        usb_transport_flush();
        CHECK(report_calls == 0);
        check_receive_empty();
        uint8_t readback[APP_HID_REPORT_SIZE];
        CHECK(tud_hid_get_report_cb(0, 0, HID_REPORT_TYPE_INPUT,
                                    readback, sizeof(readback)) == 0);
        enqueue(0x73);
        receive_marker(0x73);
    }

    reset_fixture();
    enqueue(0x74);
    const uint8_t response[APP_HID_REPORT_SIZE] = {0x75};
    usb_transport_reply(response);
    tud_resume_cb(); /* Resume itself leaves the current queue intact. */
    check_receive_empty();
    host_ready = true;
    usb_transport_flush();
    CHECK(report_calls == 1);
    CHECK(memcmp(sent_report, response, sizeof(response)) == 0);
    receive_marker(0x74);
}

int main(void) {
    test_initial_state();
    test_fifo_overflow_and_wrap();
    test_busy_tx_and_reply_order();
    test_report_lengths();
    test_unsupported_reports();
    test_get_report();
    test_usb_events();
    printf("USB transport host tests: %u checks passed.\n", checks);
    return EXIT_SUCCESS;
}
