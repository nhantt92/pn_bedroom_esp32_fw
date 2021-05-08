#ifndef __SENSOR_HUB_H__
#define __SENSOR_HUB_H__

#include <stdio.h>
#include "unity.h"
#include "bh1750.h"
#include "esp_log.h"

typedef struct
{
	uint8_t state;
	uint16_t tvoc;
	uint16_t eco2;
	float lux;
   	float temp;
   	float humi;
   	float pressure;
} sensor_struct;

extern sensor_struct sensor;

void sensor_init(void);
void sensor_task(void* arg);

void ccs811_test_init();
void ccs811_test_deinit();
void ccs811_test_getdata();

void bme280_test_init();
void bme280_test_deinit();
void bme280_test_getdata();

void bh1750_test_init();
void bh1750_test_deinit();
void bh1750_test_get_data();

#endif