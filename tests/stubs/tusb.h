#ifndef TEST_TUSB_H
#define TEST_TUSB_H

#include <stdbool.h>
#include <stdint.h>

/* Only the TinyUSB API used by usb_transport.c is supplied. In particular,
 * these host tests do not link LED control or any physical GPIO implementation.
 */
typedef enum {
    HID_REPORT_TYPE_INVALID = 0,
    HID_REPORT_TYPE_INPUT = 1,
    HID_REPORT_TYPE_OUTPUT = 2,
    HID_REPORT_TYPE_FEATURE = 3,
} hid_report_type_t;

bool tud_hid_ready(void);
bool tud_hid_report(uint8_t report_id, const void *report, uint16_t length);

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id,
                           hid_report_type_t report_type,
                           const uint8_t *buffer, uint16_t bufsize);
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id,
                              hid_report_type_t report_type,
                              uint8_t *buffer, uint16_t reqlen);
void tud_mount_cb(void);
void tud_umount_cb(void);
void tud_suspend_cb(bool remote_wakeup_en);
void tud_resume_cb(void);

#endif
