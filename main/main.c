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
#include "mlp_driver.h"


//#define TRAINING
//#define INFERENCE  

#define PIN_GPIO GPIO_NUM_4
#define PIN_INT_0 GPIO_NUM_19

#define DEMO_TASK_STACK_SIZE 8192
#define DEMO_TASK_PRIORITY 5

#define TRAINING_TASK_STACK_SIZE 4092
#define TRAINING_TASK_PRIORITY 5

static const char *TAG_MAIN = "Main";

uint8_t level = 0;
uint8_t pressing = 0;

static const float CIRCLE_CONF_THRESHOLD = 0.75f;
static const float CIRCLE_MOTION_THRESHOLD = 90.0f;

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
                cooldown_check += cooldown_time;
                //ESP_LOGI(TAG_MAIN, "BMI160 interrupt on GPIO %lu\n", io_num);
                vTaskDelay(pdMS_TO_TICKS(800));

                CIRC_BUF_DEF(buf, BUFFER_SIZE);
                i2c_get_buffer_ordered(&buf);
                
                mlp_result_t result;
                mlp_predict_buffer(&buf, &result);
                const int circle_detected =
                    (result.class_id == 1) &&
                    (result.confidence >= CIRCLE_CONF_THRESHOLD) &&
                    (result.motion_score >= CIRCLE_MOTION_THRESHOLD);

                printf("prediction=%d confidence=%.3f motion=%.1f trigger=%d\n",
                    result.class_id,
                    result.confidence,
                    result.motion_score,
                    circle_detected);

                if (circle_detected) {
                    send_keystroke(' ');
                }
            }
        }
    }
}

static void demo_w_task(void *arg) {
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

    xTaskCreate(bmi160_interrupt_task, "bmi160_task", DEMO_TASK_STACK_SIZE, NULL, DEMO_TASK_PRIORITY, NULL);
}


static void demo_inference_task(void *arg) {
	(void)arg;
    while (1) {
        level = gpio_get_level(PIN_GPIO);

        if (!level && !pressing) {
			pressing = 1;

        }
		else if (level && pressing) {
            CIRC_BUF_DEF(buf, BUFFER_SIZE);
            i2c_get_buffer_ordered(&buf);
            
            mlp_result_t result;
            mlp_predict_buffer(&buf, &result);
            const int circle_detected =
                (result.class_id == 1) &&
                (result.confidence >= CIRCLE_CONF_THRESHOLD) &&
                (result.motion_score >= CIRCLE_MOTION_THRESHOLD);

            printf("prediction=%d confidence=%.3f motion=%.1f trigger=%d\n",
                   result.class_id,
                   result.confidence,
                   result.motion_score,
                   circle_detected);

            if (circle_detected) {
                send_keystroke(' ');
            }
            
            pressing = 0;
		}

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

static void training_data_task(void *arg) {
	(void)arg;
    while (1) {
        level = gpio_get_level(PIN_GPIO);

        if (!level && !pressing) {
			pressing = 1;

        }
		else if (level && pressing) {
            CIRC_BUF_DEF(buf, BUFFER_SIZE);
            i2c_get_buffer_ordered(&buf);

            for (int i = 0; i < BUFFER_SIZE; i++) {printf("%d ", buf.buffer[i]);}
            printf("\n");

            pressing = 0;
		}

        vTaskDelay(pdMS_TO_TICKS(10));
    }
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

#ifdef INFERENCE
    BaseType_t inference_task = xTaskCreate(
        demo_inference_task,
        "demo_inference_task",
        DEMO_TASK_STACK_SIZE,
        NULL,
        DEMO_TASK_PRIORITY,
        NULL
    );

	if (inference_task != pdPASS) {
		ESP_LOGE(TAG_MAIN, "Failed to create demo_inference_task");
	}
#endif

#ifdef TRAINING
    BaseType_t training_task = xTaskCreate(
        training_data_task,
        "training_data_task",
        TRAINING_TASK_STACK_SIZE,
        NULL,
        TRAINING_TASK_PRIORITY,
        NULL
    );

	if (training_task != pdPASS) {
		ESP_LOGE(TAG_MAIN, "Failed to create training_data_task");
	}
#endif

    

}