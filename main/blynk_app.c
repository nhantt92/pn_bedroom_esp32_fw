#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdlib.h>
#include <errno.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include "freertos/event_groups.h"
#include "driver/ledc.h"
#include "driver/adc.h"
#include "esp_system.h"
#include "esp_task.h"
#include "esp_log.h"
#include "esp_event_loop.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

#include "blynk_app.h"
#include "config.h"
#include "sensor_hub.h"

#define WHITE_PWM_PIN 	4
#define WARM_PWM_PIN	5
#define PWM_BITS 10
#define PWM_FREQ 1000
#define PWM_TIMER LEDC_TIMER_0
#define PWM_MODE LEDC_HIGH_SPEED_MODE
#define PWM_CHANNEL LEDC_CHANNEL_0

uint32_t brightness1, brightness2;

device_struct device;

static const char *tag = "blynk-example";
blynk_client_t *client;

void init_pwm(void) {
	ledc_timer_config_t timer = {
		.bit_num= 10,
		.freq_hz = 1000,
		.speed_mode = LEDC_HIGH_SPEED_MODE,
		.timer_num = LEDC_TIMER_0,
	};
	ledc_timer_config(&timer);

	ledc_channel_config_t channel1 = {
		.channel = LEDC_CHANNEL_0,
		.duty = 0,
		.gpio_num = 4,
		.speed_mode = LEDC_HIGH_SPEED_MODE,
		.timer_sel = LEDC_TIMER_0,
	};
	ledc_channel_config(&channel1);

	ledc_channel_config_t channel2 = {
		.channel = LEDC_CHANNEL_1,
		.duty = 0,
		.gpio_num = 5,
		.speed_mode = LEDC_HIGH_SPEED_MODE,
		.timer_sel = LEDC_TIMER_0,
	};
	ledc_channel_config(&channel2);
}

static esp_err_t event_handler(void *arg, system_event_t *event) {
	switch (event->event_id) {
		case SYSTEM_EVENT_STA_START:
		{
			ESP_LOGI(tag, "WiFi started");
			/* Auto connect feature likely not implemented in WiFi lib, so do it manually */
			bool ac;
			//ESP_ERROR_CHECK(esp_wifi_get_auto_connect(&ac));
			if (ac) {
				//esp_wifi_connect();
			}
			esp_wifi_connect();
			break;
		}

		case SYSTEM_EVENT_STA_CONNECTED:
			/* enable ipv6 */
			tcpip_adapter_create_ip6_linklocal(TCPIP_ADAPTER_IF_STA);
			break;

		case SYSTEM_EVENT_STA_GOT_IP:
			break;

		case SYSTEM_EVENT_STA_DISCONNECTED:
			/* This is a workaround as ESP32 WiFi libs don't currently
			   auto-reassociate. */
			esp_wifi_connect();
			break;

		default:
			break;
	}
	return ESP_OK;
}

/* Blynk client state handler */
static void state_handler(blynk_client_t *c, const blynk_state_evt_t *ev, void *data) {
	ESP_LOGI(tag, "state: %d\n", ev->state);
}

/* Virtual write handler */
static void vw_handler(blynk_client_t *c, uint16_t id, const char *cmd, int argc, char **argv, void *data) {
	printf("%d\n", atoi(argv[0]));
	if (argc > 1 && atoi(argv[0]) == VP_PWM1) {
		uint32_t value = atoi(argv[1]);
		printf("value set: %d\n", value);
		sysCfg.brightness1 = value*10;
		config_save();
		if(device.lampWhite)
		{
			ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, value);
			ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0);
		}
	}
	if (argc > 1 && atoi(argv[0]) == VP_PWM2) 
	{
		uint32_t value = atoi(argv[1]);
		printf("value set: %d\n", value);
		sysCfg.brightness2 = value*10;
		config_save();
		if(device.lampWarm)
		{
			ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_1, value);
			ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_1);
		}
		
	}
	if (argc > 1 && atoi(argv[0]) == VP_TIME)
	{
		uint32_t value = atoi(argv[1]);
		printf("value set: %d\n", value);
		// printf("hour: %d, min: %d\n", value/3600, value%3600/60);
		sysCfg.hour_set = (uint8_t)(value/3600);
        sysCfg.min_set = (uint8_t)(value%3600/60);
        config_save();
        printf("hour: %d, min: %d\n", sysCfg.hour_set, sysCfg.min_set);
	}
}

/* Virtual read handler */
static void vr_handler(blynk_client_t *c, uint16_t id, const char *cmd, int argc, char **argv, void *data) {
	
	if (!argc) {
		return;
	}
	int pin = atoi(argv[0]);
	switch (pin) {
		case VP_LIGHT:
		{
			/* Respond with `virtual write' command */
			char buff[16];
        	sprintf(buff, "%.2f lx", sensor.lux);
			blynk_send(c, BLYNK_CMD_HARDWARE, 0, "sis", "vw", VP_LIGHT, buff);
			if(device.lampWhite)
			{
				blynk_send(c, BLYNK_CMD_HARDWARE, 0, "sii", "vw", VP_LED1, sysCfg.brightness1);
				blynk_send(c, BLYNK_CMD_HARDWARE, 0, "sii", "dw", LED_WHITE, 1);	
			}
			else
			{
				blynk_send(c, BLYNK_CMD_HARDWARE, 0, "sii", "dw", LED_WHITE, 0);
				blynk_send(c, BLYNK_CMD_HARDWARE, 0, "sii", "vw", VP_LED1, 0);
			}
			if(device.lampWarm)
			{
				blynk_send(c, BLYNK_CMD_HARDWARE, 0, "sii", "dw", LED_WARM, 1);
				blynk_send(c, BLYNK_CMD_HARDWARE, 0, "sii", "vw", VP_LED2, sysCfg.brightness2);
			}
			else
			{
				blynk_send(c, BLYNK_CMD_HARDWARE, 0, "sii", "dw", LED_WARM, 0);
				blynk_send(c, BLYNK_CMD_HARDWARE, 0, "sii", "vw", VP_LED2, 0);
			}
			break;

		}
		case VP_PRESSURE:
		{
			char buff[16];
        	sprintf(buff, "%.2f Pa", sensor.pressure);
			blynk_send(c, BLYNK_CMD_HARDWARE, 0, "sis", "vw", VP_PRESSURE, buff);
			break;
		}
		case VP_TEMP:
		{
			/* Get ADC value */
			//int value = adc1_get_voltage(ADC_CHANNEL);
			char buff[16];
        	sprintf(buff, "%2.2f oC", sensor.temp);

			/* Respond with `virtual write' command */
			blynk_send(c, BLYNK_CMD_HARDWARE, 0, "sis", "vw", VP_TEMP, buff);
			break;
		}
		case VP_HUMI:
		{
			//unsigned long value = (unsigned long long)xTaskGetTickCount() * portTICK_RATE_MS / 1000;
			char buff[16];
        	sprintf(buff, "%2.2f %%RH", sensor.humi);

			/* Respond with `virtual write' command */
			blynk_send(c, BLYNK_CMD_HARDWARE, 0, "sis", "vw", VP_HUMI, buff);
			break;
		}
		case VP_TVOC:
		{
			//unsigned long value = (unsigned long long)xTaskGetTickCount() * portTICK_RATE_MS / 1000;
			char buff[16];
        	sprintf(buff, "%d ppb", sensor.tvoc);

			/* Respond with `virtual write' command */
			blynk_send(c, BLYNK_CMD_HARDWARE, 0, "sis", "vw", VP_TVOC, buff);
			break;
		}
		case VP_CO2:
		{
			//unsigned long value = (unsigned long long)xTaskGetTickCount() * portTICK_RATE_MS / 1000;
			char buff[16];
        	sprintf(buff, "%d ppm", sensor.eco2);
			/* Respond with `virtual write' command */
			blynk_send(c, BLYNK_CMD_HARDWARE, 0, "sis", "vw", VP_CO2, buff);
			break;
		}
	}
}

static void dw_handler(blynk_client_t *c, uint16_t id, const char *cmd, int argc, char **argv, void *data) {
	int port = atoi(argv[0]);
	if (argc > 1)
    {
        uint32_t value = atoi(argv[1]);
        switch (port)
        {
	        case LED_WHITE:
	        	//gpio_set_level(LIGH_WHITE_PIN, atoi(argv[1]));
	        	if(value) 
	        	{
	        		device.lampWhite = 1;
	        		device.lampWarm = 0;
	        	}
	        	else
	        	{
	        		device.lampWhite = 0;
	        	}
	        	if(device.lampWhite)
	        	{
	        		ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, sysCfg.brightness1);
					ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0);
					blynk_send(c, BLYNK_CMD_HARDWARE, 0, "sii", "vw", VP_LED1, sysCfg.brightness1);
					blynk_send(c, BLYNK_CMD_HARDWARE, 0, "sii", "dw", LED_WHITE, 1);
					blynk_send(c, BLYNK_CMD_HARDWARE, 0, "sii", "dw", LED_WARM, 0);
					blynk_send(c, BLYNK_CMD_HARDWARE, 0, "sii", "vw", VP_LED2, 0);
	        	}
	        	else
	        	{
	        		ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, 0);
					ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0);
					blynk_send(c, BLYNK_CMD_HARDWARE, 0, "sii", "vw", VP_LED1, 0);
					blynk_send(c, BLYNK_CMD_HARDWARE, 0, "sii", "dw", LED_WHITE, 0);
	        	}
	        	if(device.lampWarm)
	        	{
	        		ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_1, sysCfg.brightness2);
					ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_1);
					blynk_send(c, BLYNK_CMD_HARDWARE, 0, "sii", "vw", VP_LED2, sysCfg.brightness2);
					blynk_send(c, BLYNK_CMD_HARDWARE, 0, "sii", "dw", LED_WARM, 1);
					blynk_send(c, BLYNK_CMD_HARDWARE, 0, "sii", "dw", LED_WHITE, 0);
					blynk_send(c, BLYNK_CMD_HARDWARE, 0, "sii", "vw", VP_LED1, 0);
	        	}
	        	else
	        	{
	        		ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_1, 0);
					ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_1);
					blynk_send(c, BLYNK_CMD_HARDWARE, 0, "sii", "vw", VP_LED2, 0);
					blynk_send(c, BLYNK_CMD_HARDWARE, 0, "sii", "dw", LED_WARM, 0);
	        	}
	        	printf("brightness1: %d\n", sysCfg.brightness1);
	            ESP_LOGI(tag, "write LED WHITE: %d", value);
	            break;
	        case LED_WARM:
	        	//gpio_set_level(LIGH_WARM_PIN, atoi(argv[1]));
	        	if(value)
	        	{
	        		device.lampWarm = 1;
	        		device.lampWhite = 0;
	        	}
	        	else
	        	{
	        		device.lampWarm = 0;
	        	}
	        	if(device.lampWhite)
	        	{
	        		ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, sysCfg.brightness1);
					ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0);
					blynk_send(c, BLYNK_CMD_HARDWARE, 0, "sii", "vw", VP_LED1, sysCfg.brightness1);
					blynk_send(c, BLYNK_CMD_HARDWARE, 0, "sii", "dw", LED_WHITE, 1);
					blynk_send(c, BLYNK_CMD_HARDWARE, 0, "sii", "dw", LED_WARM, 0);
					blynk_send(c, BLYNK_CMD_HARDWARE, 0, "sii", "vw", VP_LED2, 0);
	        	}
	        	else
	        	{
	        		ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, 0);
					ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0);
					blynk_send(c, BLYNK_CMD_HARDWARE, 0, "sii", "vw", VP_LED1, 0);
					blynk_send(c, BLYNK_CMD_HARDWARE, 0, "sii", "dw", LED_WHITE, 0);
	        	}
	        	if(device.lampWarm)
	        	{
	        		ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_1, sysCfg.brightness2);
					ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_1);
					blynk_send(c, BLYNK_CMD_HARDWARE, 0, "sii", "vw", VP_LED2, sysCfg.brightness2);
					blynk_send(c, BLYNK_CMD_HARDWARE, 0, "sii", "dw", LED_WARM, 1);
					blynk_send(c, BLYNK_CMD_HARDWARE, 0, "sii", "dw", LED_WHITE, 0);
					blynk_send(c, BLYNK_CMD_HARDWARE, 0, "sii", "vw", VP_LED1, 0);
	        	}
	        	else
	        	{
	        		ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_1, 0);
					ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_1);
					blynk_send(c, BLYNK_CMD_HARDWARE, 0, "sii", "vw", VP_LED2, 0);
					blynk_send(c, BLYNK_CMD_HARDWARE, 0, "sii", "dw", LED_WARM, 0);
	        	}
	        	printf("brightness2: %d\n", sysCfg.brightness2);
	            ESP_LOGI(tag, "write LED WARM: %d", value);
	            break;
		}
    }
}

static void dr_handler(blynk_client_t *c, uint16_t id, const char *cmd, int argc, char **argv, void *data) {
	ESP_LOGI(tag, "read: %s, %s", cmd, *argv);
    if (!argc)
    {
        return;
    }
    int pin = atoi(argv[0]);
}

static void wifi_conn_init()
{
    esp_wifi_set_max_tx_power(40); //Maximum WiFi transmitting power, unit is 0.25dBm, range is [40, 82]
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

    // Get config from NVS
    wifi_config_t wifi_config;
    ESP_LOGI(tag, "Load WiFi %s , %s", sysCfg.wifi_config.sta.ssid, sysCfg.wifi_config.sta.password);
    if(sysCfg.wifi_config.sta.ssid[0])
    {
        memset(&wifi_config, 0, sizeof(wifi_config));
        strlcpy((char *)wifi_config.sta.ssid, (const char *)sysCfg.wifi_config.sta.ssid, sizeof(wifi_config.sta.ssid));
        strlcpy((char *)wifi_config.sta.password, (const char *)sysCfg.wifi_config.sta.password, sizeof(wifi_config.sta.password));

        ESP_LOGI(tag, "Setting WiFi configuration SSID %s...", wifi_config.sta.ssid);
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    }
    else{
        ESP_LOGI(tag, "SSID NULL");
        return;
    }
    ESP_ERROR_CHECK(esp_wifi_start());
}

void blynk_app(void) {
	/* Init WiFi */
	init_pwm();
	device.lampWarm = 0;
	device.lampWhite = 1;
	tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));

    ESP_LOGI(tag, "init wifi");
    /* Init WiFi */
    wifi_conn_init();

    ESP_LOGI(tag, "init client: %d byte", sizeof(blynk_client_t));
	client = malloc(sizeof(blynk_client_t));
	blynk_init(client);

	blynk_options_t opt = {
		.token = BLYNK_TOKEN,
		.server = BLYNK_SERVER,
		/* Use default timeouts */
	};

	blynk_set_options(client, &opt);

	/* Subscribe to state changes and errors */
	blynk_set_state_handler(client, state_handler, NULL);

	/* blynk_set_handler sets hardware (BLYNK_CMD_HARDWARE) command handler */
	blynk_set_handler(client, "vw", vw_handler, NULL);
	blynk_set_handler(client, "vr", vr_handler, NULL);
	blynk_set_handler(client, "dw", dw_handler, NULL);
	blynk_set_handler(client, "dr", dr_handler, NULL);

	//gpio_set_direction(LIGH_WHITE_PIN, GPIO_MODE_OUTPUT);
	//gpio_set_direction(LIGH_WARM_PIN, GPIO_MODE_OUTPUT);
	/* Start Blynk client task */
	blynk_start(client);
}

void blynk_server_stop(void)
{
    esp_wifi_stop();
    vTaskDelete(client->state.task);
    free(client);
}