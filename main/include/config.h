#ifndef _CONFIG_H_
#define _CONFIG_H_

#include <stdint.h>
#include <string.h>
#include "esp_system.h"
#include "esp_wifi.h"

#define CFG_HOLDER  					  0x00FF55AB

typedef struct {
	uint32_t 		cfg_holder;
	wifi_config_t 	wifi_config;
	uint8_t			wifiConfigFlag;
	uint8_t         blynk_token[64];
	uint32_t 		brightness1;
	uint32_t		brightness2;
	uint8_t 		hour_set;
	uint8_t			min_set;
}CONFIG_POOL;

extern CONFIG_POOL sysCfg;

void config_init(void);
esp_err_t config_save(void);
esp_err_t config_load(void);
void config_setBankMode(uint8_t bankM);

#endif