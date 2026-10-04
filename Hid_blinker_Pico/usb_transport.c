#include <string.h>

#include "tusb.h"
#include "usb_transport.h"

typedef struct {
    uint8_t bytes[APP_HID_REPORT_SIZE];
    uint16_t length;
} received_report_t;

static received_report_t rx_queue[APP_HID_RX_QUEUE_DEPTH];
static size_t rx_head;
static size_t rx_count;
static uint8_t tx_report[APP_HID_REPORT_SIZE];
static uint8_t last_report[APP_HID_REPORT_SIZE];
static bool tx_pending;
static bool have_last_report;

void usb_transport_init(void) {
    rx_head = 0;
    rx_count = 0;
    tx_pending = false;
    have_last_report = false;
    memset(last_report, 0, sizeof(last_report));
}

bool usb_transport_receive(uint8_t request[APP_HID_REPORT_SIZE], size_t *length) {
    /* A stalled host must not block LED updates or watchdog servicing. */
    if (tx_pending || rx_count == 0) {
        return false;
    }
    const received_report_t *report = &rx_queue[rx_head];
    memcpy(request, report->bytes, APP_HID_REPORT_SIZE);
    *length = report->length;
    rx_head = (rx_head + 1) % APP_HID_RX_QUEUE_DEPTH;
    --rx_count;
    return true;
}

void usb_transport_reply(const uint8_t response[APP_HID_REPORT_SIZE]) {
    memcpy(tx_report, response, APP_HID_REPORT_SIZE);
    memcpy(last_report, response, APP_HID_REPORT_SIZE);
    have_last_report = true;
    tx_pending = true;
}

void usb_transport_flush(void) {
    if (tx_pending && tud_hid_ready() &&
        tud_hid_report(0, tx_report, APP_HID_REPORT_SIZE)) {
        tx_pending = false;
    }
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id,
                           hid_report_type_t report_type,
                           const uint8_t *buffer, uint16_t bufsize) {
    if (instance != 0 || report_id != 0 ||
        (report_type != HID_REPORT_TYPE_OUTPUT &&
         report_type != HID_REPORT_TYPE_INVALID)) {
        return;
    }
    /* Host contract: wait for the matching response before sending again.
     * Excess requests are discarded without applying them if the queue fills.
     */
    if (rx_count == APP_HID_RX_QUEUE_DEPTH) {
        return;
    }
    received_report_t *report =
        &rx_queue[(rx_head + rx_count) % APP_HID_RX_QUEUE_DEPTH];
    memset(report->bytes, 0, sizeof(report->bytes));
    const size_t copied = bufsize < APP_HID_REPORT_SIZE ?
                          bufsize : APP_HID_REPORT_SIZE;
    if (copied != 0) {
        memcpy(report->bytes, buffer, copied);
    }
    report->length = bufsize;
    ++rx_count;
}

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id,
                              hid_report_type_t report_type,
                              uint8_t *buffer, uint16_t reqlen) {
    if (instance != 0 || report_id != 0 ||
        report_type != HID_REPORT_TYPE_INPUT || !have_last_report) {
        return 0;
    }
    const uint16_t length = reqlen < APP_HID_REPORT_SIZE ?
                            reqlen : APP_HID_REPORT_SIZE;
    memcpy(buffer, last_report, length);
    return length;
}

/* Only clear communication buffers. LED states survive all USB events. */
void tud_mount_cb(void) {
    usb_transport_init();
}

void tud_umount_cb(void) {
    usb_transport_init();
}

void tud_suspend_cb(bool remote_wakeup_en) {
    (void)remote_wakeup_en;
    usb_transport_init();
}

void tud_resume_cb(void) {
}
