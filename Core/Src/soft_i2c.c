/*
 * soft_i2c.c
 *
 *  Created on: Dec 1, 2024
 *      Author: Jim Merkle
 *
 *  Create a Soft I2C library that should be easily adaptable to any microcontroler
 *
 * This library is similar to that created by STMicro, written in ARM assembly:
 *   https://www.st.com/resource/en/application_note/an1045-st7-sw-implementation-of-i2c-bus-master-stmicroelectronics.pdf
 * Also similar to: https://download.mikroe.com/documents/compilers/mikroc/arm/help/software_i2c_library.htm
 *
 *
 *  Timing delays between SCL falling and SCL rising  are such that it creates a 100KHz clock signal
 *  __________            __________            __________            __________            __________
 *            |          |          |          |          |          |          |          |
 *            |__________|          |__________|          |__________|          |__________|
 *
 *            |<- 5us  ->|<- 5us  ->|
 *
 * Data is only written when SCL is low, and read (from slave) when SCL is high
 *
 */

#include <stdint.h>
#include <stdbool.h>
#include "soft_i2c.h"
#include "main.h"   // HAL functions and defines for timer and GPIO access
#include <stdio.h> // printf()

// Delay a quantity of microseconds
// This can be as simple as a for-loop, counting to some number that creates 1us,
//  inside another for-loop that counts number of microseconds
// To manage counter/timer roll-over, a delta-time is always used
void i2c_delay_us(uint16_t delay_us)
{
    volatile TIM_TypeDef *TIMx = TIM4; // Establish pointer to timer 4 registers
	uint16_t start_us = TIMx->CNT; // read us hardware timer
	while((TIMx->CNT - start_us) < delay_us); // spin while delta time is less than requested time
}

void soft_i2c_init(void)
{
	// Make sure GPIO clocks are enabled for both SCL and SDA pins
	__HAL_RCC_GPIOC_CLK_ENABLE();

	/*Configure GPIO pins : Soft_SCL_Pin Soft_SDA_Pin */
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.Pin = Soft_SCL_Pin|Soft_SDA_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
	HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
}

// Implement a function to write boolean value to SCL pin such that when
//   false (0): pin is driven low
//   true  (1): pin floats, pulled high by pull-up resistor
void soft_i2c_scl_write(bool pinstate)
{
	HAL_GPIO_WritePin(Soft_SCL_GPIO_Port, Soft_SCL_Pin, (GPIO_PinState) pinstate);
}

// Implement a function to write boolean value to SDA pin such that when
//   false (0): pin is driven low
//   true  (1): pin floats, pulled high by pull-up resistor
void soft_i2c_sda_write(bool pinstate)
{
	HAL_GPIO_WritePin(Soft_SDA_GPIO_Port, Soft_SDA_Pin, (GPIO_PinState) pinstate);
}

// Implement a function to return state of SCL pin: false (0) low, true (1) high
bool soft_i2c_scl_read(void)
{
	return (bool) HAL_GPIO_ReadPin(Soft_SCL_GPIO_Port,Soft_SCL_Pin);
}

// Implement a function to return state of SDA pin: false (0) low, true (1) high
bool soft_i2c_sda_read(void)
{
	return (bool) HAL_GPIO_ReadPin(Soft_SDA_GPIO_Port,Soft_SDA_Pin);
}

// With SCL and SDA both high, lower SDA, delay, lower SCL
/* __________
*            |
*  SCL       |_____
*  _____
*       |
*  SDA  |__________
*/
void soft_i2c_start(void)
{
	soft_i2c_sda_write(false);
	i2c_delay_us(I2C_START_DELAY);
	soft_i2c_scl_write(false);
}

// With SCL and SDA both low, delay, raise SCL, delay, raise SDA
/*               ___________
*               |
*  SCL  ________|
*                    _______
*                   |
*  SDA _____________|
*/
void soft_i2c_stop(void)
{
	soft_i2c_sda_write(false); // With SCL low, force SDA low
	i2c_delay_us(I2C_SCL_LOW_DELAY);
	soft_i2c_scl_write(true);
	i2c_delay_us(I2C_STOP_DELAY);
	soft_i2c_sda_write(true);
}

// With SCL low and SDA unknown, write 8 bit value, cycle SCL again, read ACK, return ACK value
// This is used for both writing an address as well as for data bytes
// Send Most Significant Bit (MSB) first
/*      ___     ___     ___     ___     ___     ___     ___     ___     ___
*  SCL |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |
*  ____|   |___|   |___|   |___|   |___|   |___|   |___|   |___|   |___|   |___
*
* bit    7       6       5       4       3       2       1       0      ACK
*/
bool soft_i2c_write8(uint8_t data_byte)
{
	// Write 8 data bits (data or address byte)
	// Configure SDA for current bit being transmitted
	for(unsigned i=0;i<8;i++) {
		if(data_byte & 0x80)
			soft_i2c_sda_write(true);
		else
			soft_i2c_sda_write(false);
		data_byte<<=1; // left shift for next pass
		i2c_delay_us(I2C_SCL_LOW_DELAY);
		soft_i2c_scl_write(true); // SCL high, delay, low
		i2c_delay_us(I2C_SCL_HIGH_DELAY);
		soft_i2c_scl_write(false);
	}
	// Data byte has been sent, read in slave's ACK response
	soft_i2c_sda_write(true); // Allow SDA to float
	i2c_delay_us(I2C_SCL_LOW_DELAY);
	soft_i2c_scl_write(true);
	bool ack = soft_i2c_sda_read();
	i2c_delay_us(I2C_SCL_HIGH_DELAY);
	soft_i2c_scl_write(false);
	return ack;
}

// With SCL low and SDA unknown, read 8 bit value, cycle SCL again, write ACK, return byte value read
// This is used for both writing an address as well as for data bytes
// Send Most Significant Bit (MSB) first
uint8_t soft_i2c_read8(bool ack)
{
	uint8_t data_byte = 0;
	soft_i2c_sda_write(true); // allow SDA to float
	// Read 8 data bits
	// After raising SCL, read SDA for current bit being received
	for(unsigned i=0;i<8;i++) {
		i2c_delay_us(I2C_SCL_LOW_DELAY);
		soft_i2c_scl_write(true); // SCL high
		data_byte<<=1; // left shift for this pass
		if(soft_i2c_sda_read())
			data_byte |= 1; // set LSB
		// Don't need to add in 0's. We started with zero'ed data byte
		i2c_delay_us(I2C_SCL_HIGH_DELAY);
		soft_i2c_scl_write(false);
	}
	// Data byte has been sent, send slave desired ACK
	soft_i2c_sda_write(ack); // Configure SDA for ACK bit
	i2c_delay_us(I2C_SCL_LOW_DELAY);
	soft_i2c_scl_write(true);
	i2c_delay_us(I2C_SCL_HIGH_DELAY);
	soft_i2c_scl_write(false);
	return data_byte;
}

// Test for a device by writing a device address and see if the address is acknowledged
// Returns true (1) if device is present
bool i2c_device_ready(uint8_t i2c_address)
{
	soft_i2c_start();
	bool rc = soft_i2c_write8(i2c_address << 1);
	soft_i2c_stop();
	return !rc;
}

// Implement a "generic I2C API" for writing to and then reading from an I2C device (in that order)
// Initially, have both sections do their own START/STOP
int i2c_write_read(uint8_t i2c_address, uint8_t * write_data, uint8_t write_count, uint8_t * read_data, uint8_t read_count)
{
	// If write_data and write_count are non-null, perform write(s) first
	if(write_data && write_count) {
		soft_i2c_start();
		// Send address with the R/W bit set to 0, which signifies a write
		soft_i2c_write8(i2c_address << 1);
		while(write_count) {
			soft_i2c_write8(*write_data);
			write_data++;
			write_count--;
		} // while
		soft_i2c_stop();
	}// write

	// If read_data and read_count are non-null, perform read(s)
	if(read_data && read_count) {
		soft_i2c_start();
		// Send address with the R/W bit set to 1, which signifies a read
		soft_i2c_write8((i2c_address << 1) | 1);
		while(read_count) {
			*read_data = soft_i2c_read8(false);
			read_data++;
			read_count--;
		} // while
		soft_i2c_stop();
	}// read
	return 0;
}

// Perform an I2C bus scan similar to Linux's i2cdetect, or Arduino's i2c_scanner sketch
int cl_i2c_scan(void)
{
    printf("I2C Scan - scanning I2C addresses 0x%02X - 0x%02X\n",I2C_ADDRESS_MIN,I2C_ADDRESS_MAX);
    // Display Hex Header
    printf("    "); for(int i=0;i<=0x0F;i++) printf(" %0X ",i);
    // Walk through address range 0x00 - 0x77, but only test 0x03 - 0x77
    for(uint16_t addr=0;addr<=I2C_ADDRESS_MAX;addr++) {
    	// If address defines the beginning of a row, start a new row and display row text
    	if(!(addr%16)) printf("\n%02X: ",addr);
		// Check I2C addresses in the range 0x03-0x7F
		if(addr < I2C_ADDRESS_MIN || addr > I2C_ADDRESS_MAX) {
			printf("   "); // out of range
			continue;
		}
		// Perform I2C device detection - returns HAL_OK if device found
		if(i2c_device_ready(addr))
			printf("%02X ",addr);
		else
			printf("-- ");
    } // for-loop
    printf("\n");
    return 0;
} // cl_i2c_scanner

// Test routine to generate SCL/SDL waveforms for capture - I2C address followed by byte write
int cl_i2c_write(void)
{
	uint8_t index = 0;
	i2c_write_read(DS3231_ADDRESS, &index, sizeof(index), NULL, 0);
    return 0;
}

// Test routine to generate SCL/SDL waveforms for capture - I2C address followed by byte read
int cl_i2c_read(void)
{
	uint8_t data = 0xFF;
	i2c_write_read(DS3231_ADDRESS, NULL, 0, &data, sizeof(data));
    return 0;
}
