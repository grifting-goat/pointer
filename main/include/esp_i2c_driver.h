/*
 * Levi Morris
 *
 * 
 */


#ifndef ESP_I2C_DRIVER_H
#define ESP_I2C_DRIVER_H

#include <stdint.h>
#include <stddef.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "circ_buf.h"


#define I2C_MASTER_SCL_IO           1      /*!< GPIO number used for I2C master clock */
#define I2C_MASTER_SDA_IO           2      /*!< GPIO number used for I2C master data  */
#define I2C_MASTER_NUM              I2C_NUM_0                   /*!< I2C port number for master dev */
#define I2C_MASTER_FREQ_HZ          400000  /*!< I2C master clock frequency */
#define I2C_MASTER_TX_BUF_DISABLE   0                           /*!< I2C master doesn't need buffer */
#define I2C_MASTER_RX_BUF_DISABLE   0                           /*!< I2C master doesn't need buffer */
#define I2C_MASTER_TIMEOUT_MS       1000


#define BMI160_SENSOR_ADDR         0x69        /*!< Address of the BMI160 sensor */
#define BMI160_CHIP_ID_REG_ADDR   0x00        /*!< Register addresses of the "chip id" register */
#define BMI160_CMD_REG_ADDR 0x7E               /*!< Register addresses of the command register */
#define BMI160_ACC_DATA_ADDR 0x12
#define BMI160_GYRO_DATA_ADDR 0x0C
#define BMI160_ACC_CONF_REG_ADDR 0x40
#define BMI160_GYRO_CONF_REG_ADDR 0x42
#define BMI160_ACC_RANGE_REG_ADDR 0x41
#define BMI160_GYRO_RANGE_REG_ADDR 0x43
#define BMI160_ACC_NORMAL_MODE_CMD 0x11
#define BMI160_GYRO_NORMAL_MODE_CMD 0x15
#define BMI160_ACC_CONF_100HZ_NORMAL 0x28
#define BMI160_GYRO_CONF_100HZ_NORMAL 0x28
#define BMI160_ACC_RANGE_2G 0x03
#define BMI160_ACC_RANGE_4G 0x05
#define BMI160_GYRO_RANGE_2000_DPS 0x00
#define BMI160_SOFT_RESET_CMD 0xB6

#define BMI160_REG_INT_EN_0 0x50   
#define BMI160_REG_INT_OUT_CTRL 0x53 
#define BMI160_REG_INT_MAP_0    0x55
#define BMI160_REG_INT_MOTION_0    0x5F
#define BMI160_REG_INT_MOTION_1    0x60

#define BMI160_INT_MOTION_ANY (1 << 2)
#define BMI160_INT_OUTPUT_HIGH 0x0A
#define BMI160_INT_EN_0 0b00000111

#define BMI160_INT_ANYM_DUR 0x01
#define BMI160_INT_ANYM_TH   0x35

#define RECORDING_HZ 60
#define RECORDING_MS (1000 / RECORDING_HZ)
#define RECORDING_TIME 1800
#define SAMPLES_PER_CYCLE 6
#define BUFFER_SIZE ((RECORDING_HZ * RECORDING_TIME * SAMPLES_PER_CYCLE) / 1000)

#define I2C_DEMO_TASK_STACK_SIZE 4096
#define I2C_DEMO_TASK_PRIORITY 5


/**
 * @brief get a copy of the data buffer
 */
void i2c_get_buffer(Circ_buf *copy); 

/**
 * @brief get a copy of the data buffer in ordered fasion
 */
void i2c_get_buffer_ordered(Circ_buf *copy);

/**
 * @brief Main I2C function for ESP32
 */
void i2c_main(void);


#endif // ESP_I2C_DRIVER_H
