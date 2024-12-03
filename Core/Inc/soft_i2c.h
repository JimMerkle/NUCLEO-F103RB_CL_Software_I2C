/*
 * soft_i2c.h
 *
 *  Created on: Dec 1, 2024
 *      Author: Jim Merkle
 */

#ifndef INC_SOFT_I2C_H_
#define INC_SOFT_I2C_H_

#include <stdint.h>
#include <stdbool.h>
#include <stm32f1xx_hal.h> // Use F1XX HAL includes

// Our Pin Selections: GPIO.C0 and GPIO.C1
#define Soft_SCL_Pin GPIO_PIN_0
#define Soft_SCL_GPIO_Port GPIOC
#define Soft_SDA_Pin GPIO_PIN_1
#define Soft_SDA_GPIO_Port GPIOC

// Looking at STM32-F103RB, Hardware I2C, the START_DELAY and STOP_DELAY both appear to be 5us
#define I2C_SCL_LOW_DELAY   5    // us units delay, SCL LOW
#define I2C_SCL_HIGH_DELAY  5	 // us units delay, SCL HIGH
#define I2C_START_DELAY		5    // us units delay between SDA falling for Start Condition and SCL going low
#define I2C_STOP_DELAY      5    // us units delay between SCL going high and SDA going high for Stop Condition

// Defines for valid I2C slave device addresses
#define I2C_ADDRESS_MIN	0x03
#define I2C_ADDRESS_MAX 0x77

#define DS3231_ADDRESS	0x68	// 7-bit address (does not include I2C R/W bit)

void soft_i2c_start(void);
void soft_i2c_stop(void);
bool soft_i2c_write8(uint8_t data_byte);
uint8_t soft_i2c_read8(bool ack);
int i2c_write_read(uint8_t i2c_address, uint8_t * write_data, uint8_t write_count, uint8_t * read_data, uint8_t read_count);

// Command Line functions
int cl_i2c_scan(void);
int cl_i2c_write(void);
int cl_i2c_read(void);

#endif /* INC_SOFT_I2C_H_ */
