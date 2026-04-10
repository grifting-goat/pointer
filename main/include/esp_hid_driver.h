#ifndef ESP_HID_DRIVER_H
#define ESP_HID_DRIVER_H

#include <stdint.h>

#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

/* configuration */
#define PIN GPIO_NUM_4
#define KEYBOARD_INPUT_REPORT_LEN 7

/* API */
void bt_hid_main(void);
void send_keystroke(char c);
void send_mouse(uint8_t buttons, char dx, char dy, char wheel);

#ifdef __cplusplus
}
#endif

#endif // ESP_HID_DRIVER_H