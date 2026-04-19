#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_hid_driver.h"
#include "esp_i2c_driver.h"
#include "circ_buf.h"

#define PIN_GPIO GPIO_NUM_4

#define PIN_INT_0 GPIO_NUM_19

//static const char *TAG_MAIN = "Main";

uint8_t level = 0;
uint8_t pressing = 0;

TickType_t cooldown_time = pdMS_TO_TICKS(RECORDING_TIME);
TickType_t cooldown_check = pdMS_TO_TICKS(RECORDING_MS);

static QueueHandle_t gpio_evt_queue = NULL;

static void IRAM_ATTR bmi160_isr_handler(void *arg) {
    uint32_t gpio_num = (uint32_t)arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

static void bmi160_interrupt_task(void *arg) {
    uint32_t io_num;
    while (1) {
        if (xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
            TickType_t now = xTaskGetTickCount();
            if (now - cooldown_check > cooldown_time) {
                //ESP_LOGI(TAG_MAIN, "BMI160 interrupt on GPIO %lu\n", io_num);
                CIRC_BUF_DEF(buf, BUFFER_SIZE);
                i2c_get_buffer(&buf);
                for (int i = 0; i < buf.maxlen; i++) {printf("%d ", buf.buffer[i]);}
                printf("\n");
            }
        }
    }
}

void setup_gpio_interrupt() {
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));

    // Configure GPIO
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << PIN_INT_0),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,  // or use external resistor
        .intr_type    = GPIO_INTR_POSEDGE,     // rising edge for active-high
    };
    gpio_config(&io_conf);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(PIN_INT_0, bmi160_isr_handler, (void *)PIN_INT_0);


    xTaskCreate(bmi160_interrupt_task, "bmi160_task", 2048, NULL, 10, NULL);
}

void app_main(void) {
	bt_hid_main();

    i2c_main();

    setup_gpio_interrupt(); 

	gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << PIN_GPIO),   // which pin
        .mode = GPIO_MODE_INPUT,          // input mode
        .pull_up_en = GPIO_PULLUP_ENABLE, // enable pull-up
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,   // no interrupt (polling)
    };
    gpio_config(&io_conf);

	 while (1) {
        level = gpio_get_level(PIN_GPIO);

        if (!level && !pressing) {
            send_keystroke_press('w');
			pressing = 1;

        }
		else if (level && pressing) {
			send_keystroke_release('w');
			pressing = 0;
		}

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}