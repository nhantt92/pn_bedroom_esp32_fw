#ifndef __BLYNK_APP_H__
#define __BLYNK_APP_H__

#include "blynk.h"

#define ADC_BITS 10
#define ADC_WIDTH(x) ((x) - 9)
#define ADC_CHANNEL 4 /* IO32 */
#define ADC_ATTEN ADC_ATTEN_0db

#define PWM_PIN 12
#define PWM_BITS 10
#define PWM_FREQ 1000
#define PWM_TIMER LEDC_TIMER_0
#define PWM_MODE LEDC_HIGH_SPEED_MODE
#define PWM_CHANNEL LEDC_CHANNEL_0

#define BLYNK_TOKEN "ZikpeH7r7g-CwT7j2Lv9bKWqxIykvFlc"
#define BLYNK_SERVER "blynk-cloud.com"

#define LIGH_WHITE_PIN    	4
#define LIGH_WARM_PIN		5

typedef enum
{
	VP_PWM1 = 0,
	VP_PWM2 = 1,
	VP_TIME = 2,
	VP_LIGHT = 3,
	VP_PRESSURE = 4,
	VP_TEMP = 5,
	VP_HUMI = 6,
	VP_TVOC = 7,
	VP_CO2 = 8,
	VP_LED1 = 9,
	VP_LED2 = 10
} Virtual;

typedef enum
{
    LED_WHITE = 0,
    LED_WARM,
} Digital;

typedef struct
{
	uint8_t lampWhite;
	uint8_t lampWarm;
} device_struct;

extern device_struct device;

void init_pwm(void);
void blynk_app(void);
void blynk_server_stop(void);

#endif