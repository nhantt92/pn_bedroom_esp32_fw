/* Hello World Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_spi_flash.h"

#include "smartconfig.h"
#include "blynk_app.h"
#include "sensor_hub.h"

#include "driver/uart.h"
#include "driver/gpio.h"
#include "config.h"

#include "esp_sntp.h"
#include "pcf8563.h"
#include "esp_log.h"
#include "driver/ledc.h"

#define SMART_CONFIG_PIN    34
#define LED_STT_PIN         17
#define PCF_SCL_PIN     15
#define PCF_SDA_PIN     16
#define CFG_TIMEZONE    9
#define NTP_SERVER      "pool.ntp.org"

RTC_DATA_ATTR static int boot_count = 0;

device_struct device_handle;

TaskHandle_t sensor_xHandle;

void setClock(void *pvParameters)
{
    // Initialize RTC
    i2c_dev_t dev;
    if (pcf8563_init_desc(&dev, I2C_NUM_0, PCF_SDA_PIN, PCF_SCL_PIN) != ESP_OK) {
        ESP_LOGE(pcTaskGetTaskName(0), "Could not init device descriptor.");
        while (1) { vTaskDelay(1); }
    }

    struct tm time = {
        .tm_year = 2021,
        .tm_mon  = 4,  // 0-based
        .tm_mday = 6,
        .tm_hour = 17,
        .tm_min  = 43,
        .tm_sec  = 00
    };

    if (pcf8563_set_time(&dev, &time) != ESP_OK) {
        printf("Could not set time.");
        while (1) { vTaskDelay(1); }
    }
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    printf("Set initial date time done");
    boot_count = 1;
}

void getClock(void *pvParameters)
{
    // Initialize RTC
    i2c_dev_t dev;
    if (pcf8563_init_desc(&dev, I2C_NUM_0, PCF_SDA_PIN, PCF_SCL_PIN) != ESP_OK) {
        printf("Could not init device descriptor.");
        while (1) { vTaskDelay(1); }
    }

    // Get RTC date and time
    while (1) {
        struct tm rtcinfo;

        if (pcf8563_get_time(&dev, &rtcinfo) != ESP_OK) {
            printf("Could not get time.");
            while (1) { vTaskDelay(1); }
        }

        ESP_LOGI("PCF:", "%04d-%02d-%02d %02d:%02d:%02d", 
            rtcinfo.tm_year, rtcinfo.tm_mon + 1,
            rtcinfo.tm_mday, rtcinfo.tm_hour, rtcinfo.tm_min, rtcinfo.tm_sec);
        if((rtcinfo.tm_hour == sysCfg.hour_set) && (rtcinfo.tm_min == sysCfg.min_set))
        {
            device.lampWarm = 1;
            device.lampWhite = 0;
            ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_1, sysCfg.brightness2);
            ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_1);
            ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, 0);
            ledc_update_duty(LEDC_HIGH_SPEED_MODE, 0);
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

static uint8_t ligh_on = 0;
uint8_t flag = 0;

void app_main(void)
{
    uint8_t smartCfgPin;
    uint16_t smartCfgPressTime = 0;
    uint8_t smartCfgFlag = 0;
    uint8_t led = 0;
    uint8_t ledTick = 0;
    config_init();
    if (!sysCfg.wifiConfigFlag)
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    if (!sysCfg.wifiConfigFlag)
    {
        xTaskCreate(sensor_task, "sensor_task", 1024 * 5, NULL, 0, &sensor_xHandle);
        xTaskCreate(getClock, "getClock", 1024*4, NULL, 2, NULL);
        blynk_app();
    }
    else
    {
        //bat_drvInit();
        init_pwm();
        smartconfig_init();
        //xTaskCreate(dis_task, "displaytask", 1024 * 3, NULL, tskIDLE_PRIORITY + 1, &dis_xHandle);
    }
    gpio_set_direction(SMART_CONFIG_PIN, GPIO_MODE_INPUT);
    gpio_set_direction(LED_STT_PIN, GPIO_MODE_OUTPUT);
    while(1)
    {
        if (!sysCfg.wifiConfigFlag)
        {
            smartCfgPin = gpio_get_level(SMART_CONFIG_PIN);
            if (!smartCfgPin)
            {
                if (++smartCfgPressTime > 300)
                {
                    blynk_server_stop();
                    smartCfgFlag = 1;
                    sysCfg.wifiConfigFlag = 1;
                    //gpio_set_level(BAT_DVR_ON_PIN, 0);
                }
                if(smartCfgPressTime == 20)
                {
                    if(++ligh_on > 2) ligh_on = 0;
                }
            }
            else
            {
                smartCfgPressTime = 0;
            }
            switch(ligh_on)
            {
                case 0:
                    if(flag == 0)
                    {
                        device.lampWarm = 0;
                        device.lampWhite = 0;
                        ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, 0);
                        ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0);
                        ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_1, 0);
                        ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_1);
                        flag = 1;
                    }
                    else
                    {
                        device.lampWarm = device.lampWarm;
                        device.lampWhite = device.lampWhite;
                    }
                    break;
                case 1:
                    if(flag == 1)
                    {
                        device.lampWarm = 0;
                        device.lampWhite = 1;
                        ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, sysCfg.brightness1);
                        ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0);
                        ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_1, 0);
                        ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_1);
                        flag = 2;
                    }
                    else
                    {
                        device.lampWarm = device.lampWarm;
                        device.lampWhite = device.lampWhite;
                    }
                    break;
                case 2:
                    if(flag == 2)
                    {
                        ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, 0);
                        ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0);
                        ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_1, sysCfg.brightness2);
                        ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_1);
                        device.lampWarm = 1;
                        device.lampWhite = 0;
                        flag = 0;
                    }
                    else
                    {
                        device.lampWarm = device.lampWarm;
                        device.lampWhite = device.lampWhite;
                    }
                    
                    break;
                default:
                    break;
            }
            gpio_set_level(LED_STT_PIN, 0);
           
        }
        else
        {
            if (++ledTick >= 25)
            {
                ledTick = 0;
                led++;
                gpio_set_level(LED_STT_PIN, led % 2);
            }
            smartCfgPin = gpio_get_level(SMART_CONFIG_PIN);
            if (!smartCfgPin)
            {
                if (++smartCfgPressTime > 300)
                {
                    //vTaskDelete(sensor_xHandle);
                    printf("smartconfig\n");
                    smartCfgFlag = 1;
                    sysCfg.wifiConfigFlag = 0;
                }
                if(smartCfgPressTime == 20)
                {
                    if(++ligh_on > 2) ligh_on = 0;
                }
            }
            else
            {
                smartCfgPressTime = 0;
            }
            if (smartconfig_done())
            {
                printf("smartconfig\n");
                //vTaskDelete(sensor_xHandle);
                smartCfgFlag = 1;
                sysCfg.wifiConfigFlag = 0;
            }
            switch(ligh_on)
            {
                case 0:
                    device.lampWarm = 0;
                    device.lampWhite = 0;
                    ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, 0);
                    ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0);
                    ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_1, 0);
                    ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_1);
                    break;
                case 1:
                    device.lampWarm = 0;
                    device.lampWhite = 1;
                    ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, sysCfg.brightness1);
                    ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0);
                    ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_1, 0);
                    ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_1);
                    break;
                case 2:
                    ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, 0);
                    ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0);
                    ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_1, sysCfg.brightness2);
                    ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_1);
                    device.lampWarm = 1;
                    device.lampWhite = 0;
                    break;
                default:
                    break;
            }
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
        if (smartCfgFlag)
        {
            config_save();
            ESP_LOGI("MAIN", "Reboot system...");
            ESP_LOGI("MAIN", "\r\n\r\n");
            vTaskDelay(500 / portTICK_PERIOD_MS);
            esp_restart();
        }
    }
    //ESP_ERROR_CHECK(nvs_flash_init());
    //initialise_wifi();
    //smartconfig_init();
    //blynk_app();
    // printf("Hello world!\n");
}