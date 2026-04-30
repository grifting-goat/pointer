
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


#include "cnn_model.h"

// Move large buffers to static/global memory to avoid stack overflow
static float bmi160_input_buffer[GESTURE_INPUT_SIZE];
static float bmi160_circ_buf_data_space[BUFFER_SIZE];
static Circ_buf bmi160_circ_buf = {
    .buffer = bmi160_circ_buf_data_space,
    .head = 0,
    .maxlen = BUFFER_SIZE
};



//#define TRAINING
//#define INFERENCE  

#define PIN_GPIO GPIO_NUM_4
#define PIN_INT_0 GPIO_NUM_18

#define DEMO_TASK_STACK_SIZE 8192
#define INFERENCE_TASK_PRIORITY 5
#define INTERRUPT_TASK_PRIORITY 6
#define INFERENCE_TASK_CORE 1
#define AUX_TASK_CORE 0

#define TRAINING_TASK_STACK_SIZE 4092
#define TRAINING_TASK_PRIORITY 5

static const char *TAG_MAIN = "Main";

cnn_model_t * g_cnn_model;

uint8_t level = 0;
uint8_t pressing = 0;

TickType_t cooldown_time = pdMS_TO_TICKS(RECORDING_TIME * 1.4);
TickType_t cooldown_check = 0;

static QueueHandle_t gpio_evt_queue = NULL;

static void IRAM_ATTR bmi160_isr_handler(void *arg) {
    uint32_t gpio_num = (uint32_t)arg;
    xQueueOverwriteFromISR(gpio_evt_queue, &gpio_num, NULL);
}

static void bmi160_interrupt_task(void *arg) {
    uint32_t io_num;
    while (1) {
        if (xQueueReceive(gpio_evt_queue, &io_num, pdMS_TO_TICKS(100))) {
            TickType_t now = xTaskGetTickCount();
            if (now - cooldown_check > cooldown_time) {
                cooldown_check = now;
                vTaskDelay(pdMS_TO_TICKS(650));

                    i2c_get_buffer_ordered(&bmi160_circ_buf);
                    memset(bmi160_input_buffer, 0, sizeof(bmi160_input_buffer));
                    const size_t usable_len = (bmi160_circ_buf.maxlen < GESTURE_INPUT_SIZE)
                                                  ? bmi160_circ_buf.maxlen : GESTURE_INPUT_SIZE;
                    memcpy(bmi160_input_buffer,
                           bmi160_circ_buf.buffer,
                           usable_len * sizeof(bmi160_input_buffer[0]));

                cnn_result_t result = {0};
                cnn_model_infer(g_cnn_model, bmi160_input_buffer, &result);

                printf("CNN:prediction=%d confidence=%.3f motion=%.1f triggered=%d\n",
                    result.class_id,
                    result.confidence,
                    result.motion_score,
                    result.triggered
                );

                if (result.triggered) {
                    send_keystroke(' ');
                }
            }
        }
        // Always yield to avoid watchdog
        vTaskDelay(pdMS_TO_TICKS(10));
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
    gpio_evt_queue = xQueueCreate(1, sizeof(uint32_t));

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

    xTaskCreatePinnedToCore(
        bmi160_interrupt_task,
        "bmi160_task",
        DEMO_TASK_STACK_SIZE,
        NULL,
        INTERRUPT_TASK_PRIORITY,
        NULL,
        INFERENCE_TASK_CORE
    );
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
        

            if (0) {
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

            for (int i = 0; i < BUFFER_SIZE; i++) {printf("%f ", buf.buffer[i]);}
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

    g_cnn_model = cnn_model_init();
    if (!g_cnn_model) {
        ESP_LOGE(TAG_MAIN, "cnn_model_init failed; inference disabled");
        return;
    }

	gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << PIN_GPIO),   // which pin
        .mode = GPIO_MODE_INPUT,          // input mode
        .pull_up_en = GPIO_PULLUP_ENABLE, // enable pull-up
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,   // no interrupt (polling)
    };
    gpio_config(&io_conf);

    //alloc_cnn_inference_workspace();

#ifdef INFERENCE
    BaseType_t inference_task = xTaskCreatePinnedToCore(
        demo_inference_task,
        "demo_inference_task",
        DEMO_TASK_STACK_SIZE,
        NULL,
        INFERENCE_TASK_PRIORITY,
        NULL,
        INFERENCE_TASK_CORE
    );

	if (inference_task != pdPASS) {
		ESP_LOGE(TAG_MAIN, "Failed to create demo_inference_task");
	}
#endif

#ifdef TRAINING
    BaseType_t training_task = xTaskCreatePinnedToCore(
        training_data_task,
        "training_data_task",
        TRAINING_TASK_STACK_SIZE,
        NULL,
        TRAINING_TASK_PRIORITY,
        NULL,
        AUX_TASK_CORE
    );

	if (training_task != pdPASS) {
		ESP_LOGE(TAG_MAIN, "Failed to create training_data_task");
	}
#endif

    

}