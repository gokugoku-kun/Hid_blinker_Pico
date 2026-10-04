#ifndef USB_TRANSPORT_H
#define USB_TRANSPORT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "app_config.h"

/* Called only from core 0's main loop / TinyUSB task callbacks. */
void usb_transport_init(void);
bool usb_transport_receive(uint8_t request[APP_HID_REPORT_SIZE], size_t *length);
void usb_transport_reply(const uint8_t response[APP_HID_REPORT_SIZE]);
void usb_transport_flush(void);

#endif
