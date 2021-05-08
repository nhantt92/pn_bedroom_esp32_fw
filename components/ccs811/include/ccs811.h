#ifndef _CCS811_H_
#define _CCS811_H_

// #define CCS811_DEBUG_LEVEL_1	//only error messages
// #define CCS811_DEBUG_LEVEL_2	//debug and error messages

#include <stddef.h>
#include <stdio.h>

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_err.h"
#include <esp_log.h>
#include <string.h>

#include "stdint.h"
#include "stdbool.h"
#include "errno.h"
#include "i2c_bus.h"

// CCS811 I2C address
#define CCS811_I2C_ADDRESS_1	0x5A	//default
#define CCS811_I2C_ADDRESS_2	0x5B

// CCS811 clock streching counter
#define CSS811_I2C_CLOCK_STRETCH	200

// Definition of error codes
#define CCS811_OK 	0
#define CCS811_FAIL	-1

#define CCS811_INT_ERROR_MASK	0x000f
#define CCS811_DRV_ERROR_MASK	0xfff0

// Error codes for the I2C interface ORed with CCS811 driver error codes
#define CCS811_I2C_READ_FAILED	1
#define CCS811_I2C_WRITE_FAILED	2
#define CCS811_I2C_BUSY			3

// CCS811 driver error codes ORed with error codes for I2C the interface
#define CCS811_DRV_BOOT_MODE		(1 << 8) 	// firmware is in boot mode 
#define CCS811_DRV_NO_APP			(2 << 8) 	// no application firmware loaded
#define CCS811_DRV_NO_NEW_DATA		(3 << 8) 	// no new data samples are ready
#define CCS811_DRV_NO_IAQ_DATA		(4 << 8) 	// no new data samples are ready
#define CCS811_DRV_HW_ID			(5 << 8) 	// wrong hardware ID
#define CCS811_DRV_INV_SENS			(6 << 8) 	// invalid sensor ID
#define CCS811_DRV_WR_REG_INV		(7 << 8) 	// invalid register addr on write 
#define CCS811_DRV_RD_REG_INV		(8 << 8) 	// invalid register addr on read
#define CCS811_DRV_MM_INV			(9 << 8) 	// invalid meansurement mode
#define CCS811_DRV_MAX_RESIST		(10 << 8) 	// max sensor resistance reached
#define CCS811_DRV_HEAT_FAULT		(11 << 8)	// heater current no in range
#define CCS811_DRV_HEAT_SUPPLY		(12 << 8)	// heater voltage not correct
#define CCS811_DRV_WRONG_MODE		(13 << 8)	// wrong measurement mode
#define CCS811_DRV_RD_STAT_FAILED	(14 << 8) 	// read status register failed
#define CCS811_DRV_RD_DATA_FAILED	(15 << 8)	// read sensor data failed
#define CCS811_DRV_APP_START_FAIL	(16 << 8)	// sensor app start failure
#define CCS811_DRV_WRONG_PARAMS		(17 << 8)	// wrong parameters used

/* CCS811 register addresses */
#define CCS811_REG_STATUS			0x00
#define CCS811_REG_MEAS_MODE		0x01
#define CCS811_REG_ALG_RESULT_DATA	0x02
#define CCS811_REG_RAW_DATA			0x03
#define CCS811_REG_ENV_DATA			0x05
#define CCS811_REG_NTC				0x06
#define CCS811_REG_THRESHOLDS		0x10
#define CCS811_REG_BASELINE			0x11

#define CCS811_REG_HW_ID			0x20
#define CCS811_REG_HW_VER			0x21
#define CCS811_REG_FW_BOOT_VER		0x23
#define CCS811_REG_FW_APP_VER		0x24

#define CCS811_REG_ERROR_ID			0xE0

#define CCS811_REG_APP_ERASE		0xF1
#define CCS811_REG_APP_DARA			0xF2
#define CCS811_REG_APP_VERIFY		0xF3
#define CCS811_REG_APP_START		0xF4
#define CCS811_REG_SW_RESET			0xFF

// status register bits
#define CCS811_STATUS_ERROR			0x01 	//error, details in CCS811_REG_ERROR
#define CCS811_STATUS_DATA_RDY		0x08	//new data sample in ALG_RESULT_DATA
#define CCS811_STATUS_APP_VALID		0x10 	// valid application firmware loaded
#define CCS811_STATUS_FW_MODE		0x80 	// firmware is in application mode

// error register bits
#define CCS811_ERR_WRITE_REG_INV	0x01 	//invalid register address on write
#define CCS811_ERR_READ_REG_INV		0x02 	//invalid register address on read 
#define CCS811_ERR_MEASMODE_INV		0x04	//invalid requested measurement mode 
#define CCS811_ERR_MAXMODE_INV		0x08	//maximum sensor resistance exceeded
#define CCS811_ERR_HEATER_FAULT		0x10 	//heater current not in range
#define CCS811_ERR_HEATER_SUPPLY	0x20 	//heater voltage not applied correctly

// range
#define CCS_ECO2_RANGE_MIN	400
#define CCS_ECO2_RANGE_MAX	8192
#define CCS_TVOC_RANGE_MIN	0
#define CCS_TVOC_RANGE_MAX	1187

/* CCS811 operation modes */
typedef enum
{
	ccs811_mode_idle = 0, //Idle, low current mode
	ccs811_mode_1s = 1, // Constant Power mode, IAQ values every 1s
	ccs811_mode_10s = 2, // Pulse Heating mode, IAQ values every 10s
	ccs811_mode_60s = 3,  // Low Power Pulse Heating, IAQ values every 60s
	ccs811_mode_250ms = 4 // Constant Power mode, RAW data every 250ms
} ccs811_mode_t;

/* CCS811 sensor device data structure */
typedef struct
{
	int error_code; //contains the error code of last operation
	i2c_bus_device_handle_t i2c_dev;
	uint8_t dev_addr;	//I2C slave address
	ccs811_mode_t mode; //operation mode
} ccs811_dev_t;

typedef void* ccs811_handle_t;

/*The function initialized the CCS811 sensor and checks its availability*/
ccs811_handle_t ccs811_create(i2c_bus_handle_t bus, uint8_t dev_addr);
esp_err_t cc811_delete(ccs811_handle_t sensor, bool del_bus);
esp_err_t ccs811_init(ccs811_handle_t sensor);
esp_err_t ccs811_set_mode(ccs811_handle_t sensor, ccs811_mode_t mode);
esp_err_t ccs811_get_results(ccs811_handle_t sensor, uint16_t* iaq_tvoc, uint16_t* iaq_eco2, uint8_t* raw_i, uint16_t* raw_v);
uint32_t ccs811_get_ntc_resistance(ccs811_handle_t sensor, uint32_t r_ref);
esp_err_t ccs811_set_environmental_data(ccs811_handle_t sensor, float temperature, float humidity);
esp_err_t ccs811_enable_interrupt(ccs811_handle_t sensor, bool enabled);
esp_err_t ccs811_set_eco2_thresholds(ccs811_handle_t sensor, uint16_t low, uint16_t high, uint8_t hysteresis);
uint16_t ccs811_get_baseline(ccs811_handle_t sensor);
esp_err_t ccs811_set_baseline(ccs811_handle_t sensor, uint16_t baseline);

// esp_err_t ccs811_check_error_status(ccs811_handle_t sensor);
// esp_err_t ccs811_reg_read(ccs811_handle_t sensor, uint8_t reg, uint8_t *data, uint32_t len);
// esp_err_t ccs811_reg_write(ccs811_handle_t sensor, uint8_t reg, uint8_t *data, uint32_t len);
// esp_err_t ccs811_is_available(ccs811_handle_t sensor);

#endif