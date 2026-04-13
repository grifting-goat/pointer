/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */


#ifndef ESP_I2C_DRIVER_H
#define ESP_I2C_DRIVER_H

#include <stdint.h>
#include <stddef.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"


#define I2C_MASTER_SCL_IO           22      /*!< GPIO number used for I2C master clock */
#define I2C_MASTER_SDA_IO           21      /*!< GPIO number used for I2C master data  */
#define I2C_MASTER_NUM              I2C_NUM_0                   /*!< I2C port number for master dev */
#define I2C_MASTER_FREQ_HZ          400000  /*!< I2C master clock frequency */
#define I2C_MASTER_TX_BUF_DISABLE   0                           /*!< I2C master doesn't need buffer */
#define I2C_MASTER_RX_BUF_DISABLE   0                           /*!< I2C master doesn't need buffer */
#define I2C_MASTER_TIMEOUT_MS       1000


#define BMI160_SENSOR_ADDR         0x69        /*!< Address of the BMI160 sensor */
#define BMI160_CHIP_ID_REG_ADDR   0x00        /*!< Register addresses of the "chip id" register */
#define BMI160_CMD_REG_ADDR 0x7E               /*!< Register addresses of the command register */
#define BMI160_ACC_DATA_ADDR 0x12
#define BMI160_ACC_CONF_REG_ADDR 0x40
#define BMI160_ACC_RANGE_REG_ADDR 0x41
#define BMI160_ACC_NORMAL_MODE_CMD 0x11
#define BMI160_ACC_CONF_100HZ_NORMAL 0x28
#define BMI160_ACC_RANGE_2G 0x03
#define BMI160_SOFT_RESET_CMD 0xB6

#define I2C_DEMO_TASK_STACK_SIZE 4096
#define I2C_DEMO_TASK_PRIORITY 5

/**
 * @brief FreeRTOS task that initializes the BMI160 and logs peak acceleration.
 */
void i2c_demo_task(void *pvParameters);

/**
 * @brief Main I2C function for ESP32
 */
void i2c_main(void);


#endif // ESP_I2C_DRIVER_H
