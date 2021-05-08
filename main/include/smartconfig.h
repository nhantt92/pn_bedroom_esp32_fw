#ifndef __SMARTCONFIG_H__
#define __SMARTCONFIG_H__

#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_wpa2.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_smartconfig.h"

void initialise_wifi(void);
void smartconfig_task(void * parm);

void smartconfig_init();
uint8_t smartconfig_done(void);

#endif