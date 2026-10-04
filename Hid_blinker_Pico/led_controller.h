#ifndef LED_CONTROLLER_H
#define LED_CONTROLLER_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    LED_MODE_OFF = 0,
    LED_MODE_ON = 1,
    LED_MODE_BLINK = 2,
} led_mode_t;

typedef struct {
    led_mode_t mode;
    bool output_on;
    uint32_t on_ms;
    uint32_t off_ms;
    uint64_t started_us;
} led_state_t;

/* State management only; the caller applies output_on to the physical LEDs. */
void led_controller_init(void);

/* now_us must use the same monotonic microsecond clock as tick(). Invalid
 * parameters leave every LED unchanged. ON/OFF ignore the duration fields.
 * Each accepted BLINK command starts a new cycle with the LED on.
 */
bool led_controller_set(uint8_t id, led_mode_t mode, uint32_t on_ms,
                        uint32_t off_ms, uint64_t now_us);

/* Only ON and OFF are accepted. Both cancel every existing blink cycle. */
bool led_controller_set_all(led_mode_t mode);

void led_controller_tick(uint64_t now_us);

/* Returns NULL for an invalid ID. The returned state belongs to this module. */
const led_state_t *led_controller_get(uint8_t id);

#endif
