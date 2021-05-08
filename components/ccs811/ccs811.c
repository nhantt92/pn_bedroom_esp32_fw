#include "ccs811.h"

typedef struct
{
	uint8_t reserved_1 : 2;
	uint8_t int_thresh : 1; //interrupt if new ALG_RESULT_DAT crosses on of the thresholds
	uint8_t int_datardy: 1;	//interrupt if new sample is ready in ALG_RESULT_DAT
	uint8_t drive_mode : 3; //mode number binary coded
} ccs811_meas_mode_reg_t;

static esp_err_t ccs811_reg_read(ccs811_handle_t sensor, uint8_t reg, uint8_t *data, uint32_t len);
static esp_err_t ccs811_reg_write(ccs811_handle_t sensor, uint8_t reg, uint8_t *data, uint32_t len);

static esp_err_t ccs811_check_error_status(ccs811_handle_t sensor);
static esp_err_t ccs811_enable_threshold(ccs811_handle_t sensor, bool enabled);
static esp_err_t ccs811_is_available(ccs811_handle_t sensor);

ccs811_handle_t ccs811_create(i2c_bus_handle_t bus, uint8_t dev_addr)
{
	ccs811_dev_t* sensor = (ccs811_dev_t*) calloc(1, sizeof(ccs811_dev_t));
	sensor->i2c_dev = i2c_bus_device_create(bus, dev_addr, i2c_bus_get_current_clk_speed(bus));
	if (sensor->i2c_dev == NULL) {
        free(sensor);
        return NULL;
    }
	// init sensor data structure
	sensor->dev_addr = dev_addr;
	sensor->mode = ccs811_mode_idle;
	sensor->error_code = CCS811_OK;
	return (ccs811_handle_t)sensor;
}

esp_err_t cc811_delete(ccs811_handle_t sensor, bool del_bus)
{
	if(sensor == NULL) return ESP_OK;
	ccs811_dev_t* sens = (ccs811_dev_t*)sensor;
	if(del_bus) {
		i2c_bus_device_delete(sens->i2c_dev);
		sens = NULL;
	}
	free(sens);
	return ESP_OK;
}

esp_err_t ccs811_init(ccs811_handle_t sensor)
{	
	ccs811_dev_t* sens = (ccs811_dev_t*)sensor;
	if(sens == NULL) return CCS811_FAIL;
	vTaskDelay(1000/portTICK_PERIOD_MS);
	if(ccs811_is_available(sens) != CCS811_OK)
	{
		ESP_LOGI("CCS811:", "Sensor is not available");
		return CCS811_FAIL;
	}

	const static uint8_t sw_reset[4] = {0x11, 0xe5, 0x72, 0x8a};

	// doing a software reset first
	if(ccs811_reg_write(sens, CCS811_REG_SW_RESET, (uint8_t*)sw_reset, 4) != CCS811_OK)
	{
		//error_dev("Could not reset the sensor.", __FUNCTION__, sens);
		ESP_LOGI("CCS811:", "Could not reset sensor");
		return CCS811_FAIL;
	}
	uint8_t status;

	//wait 100 ms after the reset
	vTaskDelay(100/portTICK_PERIOD_MS);

	//get the status to check whether sensor is in bootloader mode
	if(ccs811_reg_read(sens, CCS811_REG_STATUS, &status, 1) != CCS811_OK)
	{
		//error_dev("Could not read status register %02x.", __FUNCTION__, sens, CCS811_REG_STATUS);
		ESP_LOGI("CCS811:", "Could not read status register %02x.", CCS811_REG_STATUS);
		return CCS811_FAIL;
	}
	printf("status_init: 0x%02x\n", status);
	// if sensor is in bootloader mode (FW_MODE == 0), it has to switch
	// to the application mode first
	if(!(status & CCS811_STATUS_FW_MODE))
	{
		//check whether valid application firmware is loaded
		if(!(status & CCS811_STATUS_APP_VALID))
		{
			//error_dev("Sensor is in boot mode, but has no valid application.", __FUNCTION__, sens);
			ESP_LOGI("CCS811:", "Sensor is in boot mode, but has no valid application");
			return CCS811_FAIL;
		}
		//switch to application mode

		if(ccs811_reg_write(sens, CCS811_REG_APP_START, 0x00, 1) != CCS811_OK)
		{
			//error_dev("Could not start application", __FUNCTION__, sens);
			ESP_LOGI("CCS811:", "Could not start application");
			return CCS811_FAIL;
		}
		//wait 100 ms after starting the app
		vTaskDelay(100/portTICK_PERIOD_MS);

		// get the status to check whether sensor switched to application mode
		if((ccs811_reg_read(sens, CCS811_REG_STATUS, &status, 1) != CCS811_OK) || !(status & CCS811_STATUS_FW_MODE))
		{
			//error_dev("Could not start application.", __FUNCTION__, sens);
			ESP_LOGI("CCS811:", "Could not start application");
			return CCS811_FAIL;
		}
		// if((ccs811_reg_read(sens, CCS811_REG_STATUS, &status, 1) != CCS811_OK))
		// {
		// 	error_dev("Could not start application.", __FUNCTION__, sens);
		// 	return CCS811_FAIL;
		// }
		printf("status_init: 0x%02x\n", status);
	}
	// try to set default measurement mode to *ccs811_mode_1s*
	vTaskDelay(100/portTICK_PERIOD_MS);
	if(ccs811_set_mode(sens, ccs811_mode_1s) != CCS811_OK)
	{
		return CCS811_FAIL;
	}
	return CCS811_OK;
}

esp_err_t ccs811_set_mode(ccs811_handle_t sensor, ccs811_mode_t mode)
{
	ccs811_dev_t* sens = (ccs811_dev_t*)sensor;
	ccs811_meas_mode_reg_t reg;
	if(sens == NULL) return CCS811_FAIL;
	sens->error_code = CCS811_OK;

	//read measurement mode register value
	uint8_t status;
	if(ccs811_reg_read(sens, CCS811_REG_MEAS_MODE, (uint8_t*)&reg, 1) != CCS811_OK)
		return CCS811_FAIL;

	reg.drive_mode = mode;

	//vTaskDelay(100/portTICK_PERIOD_MS);
	//write back measurement mode register
	if(ccs811_reg_write(sens, CCS811_REG_MEAS_MODE, (uint8_t*)&reg, 1) != CCS811_OK)
	{
		//error_dev("Could not set measurement mode.", __FUNCTION__, sens);
		ESP_LOGI("CCS811:", "Could not set measurement mode.");
		return CCS811_FAIL;
	}
	//vTaskDelay(100/portTICK_PERIOD_MS);
	// if(ccs811_write(sens, CCS811_REG_APP_START, 0x10) != CCS811_OK)
	// {
	// 	error_dev("Could not start application", __FUNCTION__, sens);
	// 	return CCS811_FAIL;
	// }

	//check whether setting measurement mode were succesfull
	if((ccs811_reg_read(sens, CCS811_REG_MEAS_MODE, (uint8_t*)&reg, 1) != CCS811_OK) || reg.drive_mode != mode)
	{
		//error_dev("Could not set measurement mode to %d", __FUNCTION__, sens, mode);
		ESP_LOGI("CCS811:", "Could not set measurement mode to %d", mode);
		return ccs811_check_error_status(sens);
	}
	sens->mode = mode;
	return CCS811_OK;
}

#define CCS811_ALG_DATA_ECO2_HB		0
#define CCS811_ALG_DATA_ECO2_LB		1
#define CCS811_ALG_DATA_TVOC_HB		2
#define CCS811_ALG_DATA_TVOC_LB		3
#define CCS811_ALG_DATA_STATUS		4
#define CCS811_ALG_DATA_ERROR_ID	5
#define CCS811_ALG_DATA_RAW_HB		6
#define CCS811_ALG_DATA_RAW_LB		7

esp_err_t ccs811_get_results(ccs811_handle_t sensor, uint16_t* iaq_tvoc, uint16_t* iaq_eco2, uint8_t* raw_i, uint16_t* raw_v)
{
	ccs811_dev_t* sens = (ccs811_dev_t*)sensor;
	if(sens == NULL) return CCS811_FAIL;
	sens->error_code = CCS811_OK;
	if(sens->mode == ccs811_mode_idle)
	{
		//error_dev("Sensor is in idle mode and not performing measurements.", __FUNCTION__, sens);
		ESP_LOGI("CCS811:", "Sensor is in idle mode and not performing measurements");
		sens->error_code = CCS811_DRV_WRONG_MODE;
		return CCS811_FAIL;
	}
	if(sens->mode == ccs811_mode_250ms && (iaq_tvoc || iaq_eco2))
	{	
		ESP_LOGI("CCS811:", "Sensor is in constant power mode, only raw data are available every 250ms");
		//error_dev("Sensor is in constant power mode, only raw data are available every 250ms", __FUNCTION__, sens);
		sens->error_code = CCS811_DRV_NO_IAQ_DATA;
		return CCS811_FAIL;
	}
	uint8_t data[8];

	//read IAQ sensor values and RAW sensor data including status and error id
	if(ccs811_reg_read(sens, CCS811_REG_ALG_RESULT_DATA, data, 8) != CCS811_OK)
	{
		//error_dev("Could not read sensor data.", __FUNCTION__, sens);
		ESP_LOGI("CCS811:", "Could not read sensor data.");
		sens->error_code |= CCS811_DRV_RD_DATA_FAILED;
		return CCS811_FAIL;
	}
	// printf("draw data:\n");
	// for(int i = 0; i < 8; i++)
	// 	printf("0x%02x\n", data[i]);
	// printf("\n");
	//check for errors
	if(data[CCS811_ALG_DATA_STATUS] & CCS811_STATUS_ERROR)
	{
		return ccs811_check_error_status(sens);
	}

	// check whether new data are ready, if not, latest values are read from sensor
	// and error_code is set
	// if(!(data[CCS811_ALG_DATA_STATUS] & CCS811_STATUS_DATA_RDY))
	// {
	// 	//debug_dev("No new data.", __FUNCTION__, sens);
	// 	ESP_LOGI("CCS811:", "No new data.");
	// 	sens->error_code = CCS811_DRV_NO_NEW_DATA;
	// }

	// if *iaq* is not NULL return IAQ sensor values;
	if(iaq_tvoc) *iaq_tvoc =  data[CCS811_ALG_DATA_TVOC_HB] << 8 | data[CCS811_ALG_DATA_TVOC_LB];
	if(iaq_eco2) *iaq_eco2 = data[CCS811_ALG_DATA_ECO2_HB] << 8 | data[CCS811_ALG_DATA_ECO2_LB];

	// if *raw* is not NULL return RAW sensor data
	if(raw_i) *raw_i = data[CCS811_ALG_DATA_RAW_HB] >> 2;
	if(raw_v) *raw_v = (data[CCS811_ALG_DATA_RAW_HB] & 0x03) << 8 | data[CCS811_ALG_DATA_RAW_LB];

	return CCS811_OK; 
}

uint32_t ccs811_get_ntc_resistance(ccs811_handle_t sensor, uint32_t r_ref)
{
	ccs811_dev_t* sens = (ccs811_dev_t*)sensor;
	if(sens == NULL) return CCS811_FAIL;
	uint8_t data[4];
	//read baseline register
	if(ccs811_reg_read(sens, CCS811_REG_NTC, data, 4) != CCS811_OK)
		return 0;
	// calculation from application note asm AN000372
	uint16_t v_ref = (uint16_t)(data[0]) << 8 | data[1];
	uint16_t v_ntc = (uint16_t)(data[2]) << 8 | data[3];

	return (v_ntc * r_ref / v_ref);
}

esp_err_t ccs811_set_environmental_data(ccs811_handle_t sensor, float temperature, float humidity)
{
	ccs811_dev_t* sens = (ccs811_dev_t*)sensor;
	if(sens == NULL) return CCS811_FAIL;
	uint16_t temp = (temperature + 25)*512; // -25 °C maps to 0
	uint16_t humi = humidity * 512;

	// fill environmental data
	uint8_t data[4] = {temp >> 8, temp & 0xff, humi >> 8, humi & 0xff};
	// send environmental data to the sensor
	if(ccs811_reg_write(sens, CCS811_REG_ENV_DATA, data, 4) != CCS811_OK)
	{
		//error_dev("Could not write environmental data to sensor.", __FUNCTION__, sens);
		ESP_LOGI("CCS811:", "Could not write environmental data to sensor.");
		return CCS811_FAIL;
	}
	return CCS811_OK;
}

esp_err_t ccs811_set_eco2_thresholds(ccs811_handle_t sensor, uint16_t low, uint16_t high, uint8_t hysteresis)
{
	ccs811_dev_t* sens = (ccs811_dev_t*)sensor;
	if(sens == NULL) return CCS811_FAIL;
	sens->error_code = CCS811_OK;

	//check whether interrupt has to be disabled
	if(!low && !high && !hysteresis)
		return ccs811_enable_threshold(sens, false);

	//check parameters
	if(low < CCS_ECO2_RANGE_MIN || high > CCS_ECO2_RANGE_MAX || low > high || !hysteresis)
	{
		//error_dev("Wrong threshold parameters", __FUNCTION__, sens);
		ESP_LOGI("CCS811:", "Wrong threshold parameters");
		sens->error_code = CCS811_DRV_WRONG_PARAMS;
		return ccs811_enable_threshold(sens, false);
	}

	//fill the threshold data
	uint8_t data[5] = {low >> 8, low & 0xff, high >> 8, high & 0xff, hysteresis};
	//write threshold data to the sensor
	if(ccs811_reg_write(sens, CCS811_REG_THRESHOLDS, data, 5) != CCS811_OK)
	{
		//error_dev("Could not warite threshold interrupt data to sensor.", __FUNCTION__, sens);
		ESP_LOGI("CCS811:", "Could not warite threshold interrupt data to sensor.");
		return ccs811_enable_threshold(sens, false);
	}
	// finally enable the threshold interrupt mode
	return ccs811_enable_threshold(sens, true);
}

esp_err_t ccs811_enable_interrupt(ccs811_handle_t sensor, bool enabled)
{
	ccs811_dev_t* sens = (ccs811_dev_t*)sensor;
	if(sens == NULL) return CCS811_FAIL;
	ccs811_meas_mode_reg_t reg;

	//read measurement mode register value
	if(!ccs811_reg_read(sens, CCS811_REG_MEAS_MODE, (uint8_t*)&reg, 1))
		return CCS811_FAIL;
	reg.int_datardy = enabled;
	reg.int_thresh = false; // threshold mode must not enabled

	//write back measurement mode register
	if(ccs811_reg_write(sens, CCS811_REG_MEAS_MODE, (uint8_t*)&reg, 1) != CCS811_OK)
	{
		//error_dev("Could not set measurement mode register.", __FUNCTION__, sens);
		ESP_LOGI("CCS811:", "Could not set measurement mode register.");
		return CCS811_FAIL;
	}
	return CCS811_OK;
}

uint16_t ccs811_get_baseline(ccs811_handle_t sensor)
{
	ccs811_dev_t* sens = (ccs811_dev_t*)sensor;
	if(sens == NULL) return CCS811_FAIL;
	uint8_t data[2];

	// read baseline register
	if(ccs811_reg_read(sens, CCS811_REG_BASELINE, data, 2) != CCS811_OK)
		return CCS811_FAIL;
	return (uint16_t)(data[0]) << 8 | data[1];
}

esp_err_t ccs811_set_baseline(ccs811_handle_t sensor, uint16_t baseline)
{
	ccs811_dev_t* sens = (ccs811_dev_t*)sensor;
	if(sens == NULL) return CCS811_FAIL;
	uint8_t data[2] = {baseline >> 8, baseline & 0xff};

	//write baseline register
	if(ccs811_reg_write(sens, CCS811_REG_BASELINE, data, 2)	!= CCS811_OK)
		return CCS811_FAIL;
	return CCS811_OK;
}

static esp_err_t ccs811_enable_threshold(ccs811_handle_t sensor, bool enabled)
{
	ccs811_dev_t* sens = (ccs811_dev_t*)sensor;
	if(sens == NULL) return CCS811_FAIL;
	ccs811_meas_mode_reg_t reg;

	//first, enable/disbale the data ready interrupt
	if(ccs811_enable_interrupt(sens, enabled) != CCS811_OK)
		return CCS811_FAIL;

	//read measurement mode register value
	if(ccs811_reg_read(sens, CCS811_REG_MEAS_MODE, (uint8_t*)&reg, 1) != CCS811_OK)
		return CCS811_FAIL;

	//second, enable/disable the threshold interrupt mode
	reg.int_thresh = enabled;

	//write back measurement mode register
	if(ccs811_reg_write(sens, CCS811_REG_MEAS_MODE, (uint8_t*)&reg, 1) != CCS811_OK)
	{
		//error_dev("Could not set measurement mode register.", __FUNCTION__, sens);
		ESP_LOGI("CCS811:", "Could not set measurement mode register.");
		return CCS811_FAIL;
	}
	return CCS811_OK;
}


static esp_err_t ccs811_reg_read(ccs811_handle_t sensor, uint8_t reg, uint8_t *data, uint32_t len)
{
	ccs811_dev_t* sens = (ccs811_dev_t*)sensor;
	if((sens == NULL) || !data) return CCS811_FAIL;
	//debug_dev("Read %d byte from i2c slave starting at reg addr %02x.", __FUNCTION__, sens, len, reg);
	ESP_LOGI("CCS811:", "Read %d byte from i2c slave starting at reg addr %02x.", len, reg);
	esp_err_t result;
	// i2c_cmd_handle_t cmd = i2c_cmd_link_create();
	// i2c_master_start(cmd);
	// i2c_master_write_byte(cmd, (sens->addr << 1) | I2C_MASTER_WRITE, ACK_CHECK_EN);
	// i2c_master_write_byte(cmd, reg, ACK_CHECK_EN);
	// i2c_master_start(cmd);
	// i2c_master_write_byte(cmd, (sens->addr << 1) | I2C_MASTER_READ, ACK_CHECK_EN);
	// if(len > 1) i2c_master_read(cmd, data, len-1, I2C_ACK_VAL);
	// i2c_master_read_byte(cmd, data + len -1, I2C_NACK_VAL);
	// i2c_master_stop(cmd);
	// result = i2c_bus_cmd_begin(sens->bus, cmd, 10000/portTICK_RATE_MS);
	// i2c_cmd_link_delete(cmd);
	//result = i2c_master_read_slave(sens->bus, sens->addr, reg, data, len);
	result = i2c_bus_read_bytes(sens->i2c_dev, reg, len, data);
	if(result != ESP_OK)
	{
		sens->error_code |= (result == -EBUSY) ? CCS811_I2C_BUSY : CCS811_I2C_READ_FAILED;
		//error_dev("Error %d on read %d byte from I2C slave reg addr %02x.", __FUNCTION__, sens, result, len, reg);
		ESP_LOGI("CCS811:", "Error %d on read %d byte from I2C slave reg addr %02x.", result, len, reg);
		return CCS811_FAIL;
	}
	#ifdef CCS811_DEBUG_LEVEL_2
		printf("CCS811 %s: bus %p, addr %02x - Read following bytes:", __FUNCTION__, sens->bus, sens->addr);
		printf("%0x: ", reg);
		for(int i = 0; i < len; i++)
			printf("%0x", data[i]);
		printf("\n");
	#endif
	return CCS811_OK;
}

static esp_err_t ccs811_reg_write(ccs811_handle_t sensor, uint8_t reg, uint8_t *data, uint32_t len)
{
	ccs811_dev_t* sens = (ccs811_dev_t*)sensor;
	if(sens == NULL) return CCS811_FAIL;
	//debug_dev("Write %d bytes to i2c slave starting at reg addr %02x", __FUNCTION__, sens, len, reg);
	ESP_LOGI("CCS811:", "Write %d bytes to i2c slave starting at reg addr %02x", len, reg);
	#ifdef CCS811_DEBUG_LEVEL_2
	if(data && len)
	{
		printf("CCS811 %s: bus %p, addr %02x - Write following bytes: ", __FUNCTION__, sens->bus, sens->addr);
		for(int i = 0; i < len; i++)
			printf("%02x ", data[i]);
		printf("\n");
	}
	#endif
	// i2c_cmd_handle_t cmd = i2c_cmd_link_create();
	// i2c_master_start(cmd);
	// i2c_master_write_byte(cmd, (sens->addr << 1) | I2C_MASTER_WRITE, ACK_CHECK_EN);
	// if(reg)
	// 	i2c_master_write_byte(cmd, reg, ACK_CHECK_EN);
	// if(data != NULL)
	// 	i2c_master_write(cmd, data, len, ACK_CHECK_EN);
	// i2c_master_stop(cmd);
	// esp_err_t result = i2c_bus_cmd_begin(sens->bus, cmd, 10000/portTICK_RATE_MS);
	// i2c_cmd_link_delete(cmd);
	esp_err_t result = i2c_master_write_slave(sens->i2c_dev, sens->dev_addr, reg, data, len);
	//esp_err_t result = i2c_bus_write_bytes(sens->i2c_dev, reg, len, data);
	if(result != ESP_OK)
	{
		sens->error_code |= (result == -EBUSY) ? CCS811_I2C_BUSY : CCS811_I2C_WRITE_FAILED;
		//error_dev("Error %d on write %d byte to i2c slave register %02x.", __FUNCTION__, sens, result, len, reg);
		ESP_LOGI("CCS811:", "Error %d on write %d byte to i2c slave register %02x.", result, len, reg);
		return CCS811_FAIL;
	}
	return CCS811_OK;
}

static esp_err_t ccs811_check_error_status(ccs811_handle_t sensor)
{
	ccs811_dev_t* sens = (ccs811_dev_t*)sensor;
	if(sens == NULL) return CCS811_FAIL;
	sens->error_code = CCS811_OK;
	uint8_t status;
	uint8_t err_reg;

	// check status register
	if(ccs811_reg_read(sens, CCS811_REG_STATUS, &status, 1) != CCS811_OK)
		return CCS811_FAIL;

	printf("REG_status: 0x%02x\n", status);
	if(!status & CCS811_STATUS_ERROR)
		// everything is fine
		return CCS811_OK;

	// Check the error register
	if(ccs811_reg_read(sens, CCS811_REG_ERROR_ID, &err_reg, 1) != CCS811_OK)
		return CCS811_FAIL;
	printf("check_status REG_Error: 0x%02x\n", err_reg);
	if(err_reg & CCS811_ERR_WRITE_REG_INV)
	{
		//error_dev("Received an invalid register for write.", __FUNCTION__, sens);
		ESP_LOGI("CCS811:", "Received an invalid register for write.");
		sens->error_code = CCS811_DRV_WR_REG_INV;
		return CCS811_FAIL;
	}

	if(err_reg & CCS811_ERR_READ_REG_INV)
	{
		//error_dev("Received an invalid register for read.", __FUNCTION__, sens);
		ESP_LOGI("CCS811:", "Received an invalid register for read.");
		sens->error_code = CCS811_DRV_RD_REG_INV;
		return CCS811_FAIL;
	}

	if(err_reg & CCS811_ERR_MEASMODE_INV)
	{
		//error_dev("Received and invalid measurement mode request.", __FUNCTION__, sens);
		ESP_LOGI("CCS811:", "Received and invalid measurement mode request");
		sens->error_code = CCS811_DRV_MM_INV;
		return CCS811_FAIL;
	}

	if(err_reg & CCS811_DRV_MAX_RESIST)
	{
		//error_dev("Sensor resistance measurement has reached or exceeded the maximum range.", __FUNCTION__, sens);
		ESP_LOGI("CCS811:", "Sensor resistance measurement has reached or exceeded the maximum range.");
		sens->error_code = CCS811_DRV_MAX_RESIST;
		return CCS811_FAIL;
	}

	if(err_reg & CCS811_ERR_HEATER_FAULT)
	{
		//error_dev("Heater current not in range.", __FUNCTION__, sens);
		ESP_LOGI("CCS811:", "Heater current not in range");
		sens->error_code = CCS811_DRV_HEAT_FAULT;
		return CCS811_FAIL;
	}

	if(err_reg & CCS811_ERR_HEATER_SUPPLY)
	{
		//error_dev("Heater voltage is not being applied correctly.", __FUNCTION__, sens);
		ESP_LOGI("CCS811:", "Heater voltage is not being applied correctly.");
		sens->error_code = CCS811_DRV_HEAT_SUPPLY;
		return CCS811_FAIL;
	}

	return CCS811_OK;
}

static esp_err_t ccs811_is_available(ccs811_handle_t sensor)
{
	ccs811_dev_t* sens = (ccs811_dev_t*)sensor;
	if(!sens) return CCS811_FAIL;
	sens->error_code = CCS811_OK;
	uint8_t reg_data[5];
	//check hardware id (register 0x20) and hardware version (register 0x21)
	if(ccs811_reg_read(sens, CCS811_REG_HW_ID, reg_data, 5) != ESP_OK)
		return CCS811_FAIL;
	if(reg_data[0] != 0x81)
	{
		//error_dev("Wrong hardware ID %02x, should be 0x81", __FUNCTION__, sens, reg_data[0]);
		ESP_LOGI("CCS811:", "Wrong hardware ID %02x, should be 0x81.", reg_data[0]);
		sens->error_code = CCS811_REG_HW_ID;
		return CCS811_FAIL;
	}
	ESP_LOGI("CCS811:", "hardware version: %02x", reg_data[1]);
	ESP_LOGI("CCS811:", "firmware boot version: %02x", reg_data[3]);
	ESP_LOGI("CCS811:", "firmware app version: %02x", reg_data[4]);
	return ccs811_check_error_status(sens);
}
