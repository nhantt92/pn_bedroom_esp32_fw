#include "config.h"
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "blynk_app.h"

#define STORAGE_NAMESPACE "storage"
static const char *TAG = "CFG";
CONFIG_POOL sysCfg;

esp_err_t config_load(void)
{
    nvs_handle my_handle;
    esp_err_t err;
    // Open
    err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle);
    ESP_ERROR_CHECK(err);
    if (err != ESP_OK) return err;
    // Read run time blob
    size_t required_size = 0;  // value will default to 0, if not set yet in NVS
    // obtain required memory space to store blob being read from NVS
    err = nvs_get_blob(my_handle, "save_data", NULL, &required_size);
    //ESP_ERROR_CHECK(err);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) return err;
    ESP_LOGI(TAG, "save_data: %d", required_size);
    if (required_size == 0) {
        ESP_LOGI(TAG, "Nothing saved yet!");
    }
    else {
        uint32_t* dataGet = malloc(required_size);
        err = nvs_get_blob(my_handle, "save_data", dataGet, &required_size);
        ESP_ERROR_CHECK(err);
        if (err != ESP_OK) return err;
        memcpy(&sysCfg, dataGet, sizeof sysCfg);
        ESP_LOGI(TAG, "Holder: %08x", sysCfg.cfg_holder);
        free(dataGet);
    }
    // Close
    nvs_close(my_handle);
    return ESP_OK;
}

esp_err_t config_save(void)
{
    nvs_handle my_handle;
    esp_err_t err;

    // Open
    err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle);
    ESP_ERROR_CHECK(err);
    if (err != ESP_OK) return err;

    // Read the size of memory space required for blob
    size_t required_size = 0;  // value will default to 0, if not set yet in NVS
    err = nvs_get_blob(my_handle, "save_data", NULL, &required_size);
    //ESP_ERROR_CHECK(err);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) return err;
    ESP_LOGI(TAG, "Read the size of memory space required: %d", required_size);
    ESP_LOGI(TAG, "Heap: %ld", (unsigned long)esp_get_free_heap_size());
    // Read previously saved blob if available
    uint32_t* dataSave = malloc(sizeof sysCfg);
    if (required_size > 0) {
        err = nvs_get_blob(my_handle, "save_data", dataSave, &required_size);
        ESP_ERROR_CHECK(err);
        if (err != ESP_OK) return err;
    }

    // Write value including previously saved blob if available
    required_size = sizeof sysCfg;
    memcpy(dataSave, &sysCfg, sizeof sysCfg);
    //dataSave[required_size / sizeof(uint32_t) - 1] = 0xFFFF0000;
    ESP_LOGI(TAG, "Write value including previously saved blob: %d", required_size);
    err = nvs_set_blob(my_handle, "save_data", dataSave, required_size);
    ESP_ERROR_CHECK(err);
    free(dataSave);
    if (err != ESP_OK) return err;
    // Commit
    err = nvs_commit(my_handle);
    ESP_ERROR_CHECK(err);
    if (err != ESP_OK) return err;
    // Close
    nvs_close(my_handle);
    return ESP_OK;
}

void config_setBankMode(uint8_t bankM)
{

}

void config_init(void)
{
    esp_err_t err;
    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    err = config_load();
    if (err != ESP_OK)
        ESP_LOGI(TAG, "Error (%d) reading data from NVS!", err);
    if(sysCfg.cfg_holder != CFG_HOLDER){
        ESP_LOGI(TAG, "Add default config");
        /*Add default configurations here*/
        memset(&sysCfg, 0x00, sizeof sysCfg);
		sysCfg.cfg_holder = CFG_HOLDER;
        sprintf((char*)sysCfg.wifi_config.sta.ssid, "%s", "PhuongHai");
        sprintf((char*)sysCfg.wifi_config.sta.password, "%s", "phuongHai123");
        sysCfg.wifiConfigFlag = 0;
        sysCfg.brightness1 = 50;
        sysCfg.brightness2 = 50;
        sysCfg.hour_set = 22;
        sysCfg.min_set = 00;
        sprintf((char*)sysCfg.blynk_token, "%s", BLYNK_TOKEN);
        err = config_save();
        if (err != ESP_OK)
            ESP_LOGI(TAG, "Error (%d) saving data to NVS!", err);
    }
}