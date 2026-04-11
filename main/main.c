#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"

#include "driver/gpio.h"

#include "esp_hid_driver.h"

int level = 0;

void app_main(void) {
	bt_hid_main();

	gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << PIN),   // which pin
        .mode = GPIO_MODE_INPUT,          // input mode
        .pull_up_en = GPIO_PULLUP_ENABLE, // enable pull-up
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,   // no interrupt (polling)
    };
    gpio_config(&io_conf);

	 while (1) {
        level = gpio_get_level(PIN);

        if (!level) {
            send_keystroke('w');
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}