/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
/* i2c - Simple Example


   Simple I2C example that shows how to initialize I2C
   as well as reading and writing from and to registers for a sensor connected over I2C.


   The sensor used in this example is a BMI160 inertial measurement unit.
*/
#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "include/esp_i2c_driver.h"


static const char *TAG = "example";

typedef struct {
    i2c_master_bus_handle_t  bus;
    i2c_master_dev_handle_t  dev;
    SemaphoreHandle_t        mutex;
} i2c_shared_t;


i2c_shared_t shared;


/**
 * @brief Read a sequence of bytes from a BMI160 sensor registers
 */
static esp_err_t bmi160_register_read(i2c_master_dev_handle_t dev_handle, uint8_t reg_addr, uint8_t *data, size_t len)
{
    return i2c_master_transmit_receive(dev_handle, &reg_addr, 1, data, len, I2C_MASTER_TIMEOUT_MS);
}


/**
 * @brief Write a byte to a BMI160 sensor register
 */
static esp_err_t bmi160_register_write_byte(i2c_master_dev_handle_t dev_handle, uint8_t reg_addr, uint8_t data)
{
    uint8_t write_buf[2] = {reg_addr, data};
    return i2c_master_transmit(dev_handle, write_buf, sizeof(write_buf), I2C_MASTER_TIMEOUT_MS);
}


/**
 * @brief i2c master initialization
 */
static void i2c_master_init(i2c_master_bus_handle_t *bus_handle, i2c_master_dev_handle_t *dev_handle)
{
    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_MASTER_NUM,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, bus_handle));


    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = BMI160_SENSOR_ADDR,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(*bus_handle, &dev_config, dev_handle));


    shared.bus = *bus_handle;
    shared.dev = *dev_handle;

}

esp_err_t esp_i2c_get_accel(int16_t* acc_buffer) {
    uint8_t buffer[6] = {0};
    esp_err_t err = bmi160_register_read(shared.dev, BMI160_ACC_DATA_ADDR, buffer, 6);
    if (err != ESP_OK) {return err;}
    for (int i = 0; i < 3; i++) {
        acc_buffer[i] = (int16_t)((buffer[(i * 2) + 1] << 8) | buffer[i * 2]);
    }

    return ESP_OK;

}


static void shared_init() {

    shared.mutex = xSemaphoreCreateMutex();
    assert(shared.mutex != NULL);

    i2c_master_bus_handle_t bus_handle;
    i2c_master_dev_handle_t dev_handle;

    i2c_master_init(&bus_handle, &dev_handle);

}


void i2c_demo_task(void *pvParameters)
{
    (void)pvParameters;

    esp_err_t err;

    int16_t raw_acceleration[3] = {0};
    int16_t max_acceleration[3] = {0};
    int64_t max_acceleration_mag = 0;
    

    TickType_t sample_time = pdMS_TO_TICKS(10);
    TickType_t print_time = pdMS_TO_TICKS(1000);

    TickType_t print_mark = xTaskGetTickCount();

    while (esp_i2c_get_accel(raw_acceleration) == ESP_OK) {

        int64_t max_new_acceleration_mag = 0;
        for (int i = 0; i < 3; i++) {
            max_new_acceleration_mag += ((int64_t)raw_acceleration[i] * raw_acceleration[i]);
        }

        if (max_new_acceleration_mag > max_acceleration_mag) {
            max_acceleration_mag = max_new_acceleration_mag;
            for (int i = 0; i < 3; i++) {
                max_acceleration[i] = raw_acceleration[i];
            }
        }

        TickType_t now = xTaskGetTickCount();

        if ((now - print_mark) > print_time) {

            int32_t ax_mg = ((int32_t)max_acceleration[0] * 1000) / 16384;
            int32_t ay_mg = ((int32_t)max_acceleration[1] * 1000) / 16384;
            int32_t az_mg = ((int32_t)max_acceleration[2] * 1000) / 16384;

            max_acceleration_mag = 0;

            print_mark += print_time;

            ESP_LOGI(TAG, "Accel (mg) X:%ld Y:%ld Z:%ld", (long)ax_mg, (long)ay_mg, (long)az_mg);

        }
        

        vTaskDelay(sample_time);

    }

    ESP_LOGW(TAG, "Accelerometer read loop stopped");


    /* Demonstrate writing by resetting the BMI160 */
    err = bmi160_register_write_byte(shared.dev, BMI160_CMD_REG_ADDR, BMI160_SOFT_RESET_CMD);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "BMI160 soft reset command failed: %s", esp_err_to_name(err));
    }

    err = i2c_master_bus_rm_device(shared.dev);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to remove I2C device: %s", esp_err_to_name(err));
    }
    err = i2c_del_master_bus(shared.bus);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to delete I2C bus: %s", esp_err_to_name(err));
    }
    ESP_LOGI(TAG, "I2C de-initialized successfully");

    vTaskDelete(NULL);
}


void i2c_main()
{

    uint8_t data[2];

    shared_init();

    esp_err_t err;
    
    ESP_LOGI(TAG, "I2C initialized successfully");


    /* Read the BMI160 CHIP_ID register, on power up the register should have the value 0xD1 */
    err = bmi160_register_read(shared.dev, BMI160_CHIP_ID_REG_ADDR, data, 1);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read BMI160 CHIP_ID: %s", esp_err_to_name(err));
        err = i2c_master_bus_rm_device(shared.dev);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Failed to remove I2C device: %s", esp_err_to_name(err));
        }
        err = i2c_del_master_bus(shared.bus);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Failed to delete I2C bus: %s", esp_err_to_name(err));
        }
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG, "CHIP_ID = %X", data[0]);


    err = bmi160_register_write_byte(shared.dev, BMI160_CMD_REG_ADDR, BMI160_ACC_NORMAL_MODE_CMD);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set accel normal mode: %s", esp_err_to_name(err));
        err = i2c_master_bus_rm_device(shared.dev);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Failed to remove I2C device: %s", esp_err_to_name(err));
        }
        err = i2c_del_master_bus(shared.bus);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Failed to delete I2C bus: %s", esp_err_to_name(err));
        }
        vTaskDelete(NULL);
        return;
    }

    vTaskDelay(pdMS_TO_TICKS(5));

    err = bmi160_register_write_byte(shared.dev, BMI160_ACC_CONF_REG_ADDR, BMI160_ACC_CONF_100HZ_NORMAL);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to set accel config: %s", esp_err_to_name(err));
    }
    err = bmi160_register_write_byte(shared.dev, BMI160_ACC_RANGE_REG_ADDR, BMI160_ACC_RANGE_2G);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to set accel range: %s", esp_err_to_name(err));
    }


    BaseType_t task_created = xTaskCreate(
        i2c_demo_task,
        "i2c_demo_task",
        I2C_DEMO_TASK_STACK_SIZE,
        NULL,
        I2C_DEMO_TASK_PRIORITY,
        NULL);

    if (task_created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create i2c_demo_task");
    }
}