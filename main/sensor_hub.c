#include "sensor_hub.h"

#include <stdio.h>
#include "unity.h"
#include "esp_log.h"
#include "driver/i2c.h"
#include "bme280.h"
#include "i2c_bus.h"

#include "ccs811.h"

#define I2C_MASTER_SCL_IO           22          /*!< gpio number for I2C master clock IO21*/
#define I2C_MASTER_SDA_IO           21          /*!< gpio number for I2C master data  IO15*/
#define I2C_MASTER_NUM              I2C_NUM_1   /*!< I2C port number for master bme280 */
#define I2C_MASTER_TX_BUF_DISABLE   0           /*!< I2C master do not need buffer */
#define I2C_MASTER_RX_BUF_DISABLE   0           /*!< I2C master do not need buffer */
#define I2C_MASTER_FREQ_HZ          100000      /*!< I2C master clock frequency */

sensor_struct sensor;
static i2c_bus_handle_t i2c_bus = NULL;
static bme280_handle_t bme280 = NULL;
static ccs811_handle_t ccs811 = NULL;
static bh1750_handle_t bh1750 = NULL;

void sensor_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    i2c_bus = i2c_bus_create(I2C_MASTER_NUM, &conf);
    sensor.state = 0;
}

void sensor_task(void* arg)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    i2c_bus = i2c_bus_create(I2C_MASTER_NUM, &conf);
    // bme280_test_init();
    bme280 = bme280_create(i2c_bus, BME280_I2C_ADDRESS_DEFAULT);
    ESP_LOGI("BME280:", "bme280_default_init:%d", bme280_default_init(bme280));
    ccs811 = ccs811_create(i2c_bus, CCS811_I2C_ADDRESS_1);
    ESP_LOGI("CCS811:", "ccs811_init:%d", ccs811_init(ccs811));
    bh1750 = bh1750_create(i2c_bus, BH1750_I2C_ADDRESS_DEFAULT);
    int ret;
    bh1750_cmd_measure_t cmd_measure;
    while(1)
    {
        if (ESP_OK == bme280_read_temperature(bme280, &sensor.temp))
            ESP_LOGI("BME280", "temperature:%f ", sensor.temp);
        vTaskDelay(100 / portTICK_RATE_MS);
        if (ESP_OK == bme280_read_humidity(bme280, &sensor.humi))
            ESP_LOGI("BME280", "humidity:%f ", sensor.humi);
        vTaskDelay(100 / portTICK_RATE_MS);
        if (ESP_OK == bme280_read_pressure(bme280, &sensor.pressure))
            ESP_LOGI("BME280", "pressure:%f\n", sensor.pressure);
        vTaskDelay(100 / portTICK_RATE_MS);
        if(!ccs811_get_results(ccs811, &sensor.tvoc, &sensor.eco2, 0, 0))
        {
            //printf("CCS811 sensor: TVOC: %d ppb, eCO2 %d ppm\n", sensor.tvoc, sensor.eco2);
        }
        vTaskDelay(100 / portTICK_RATE_MS);
        cmd_measure = BH1750_CONTINUE_4LX_RES;
        bh1750_set_measure_mode(bh1750, cmd_measure);
        vTaskDelay(30 / portTICK_RATE_MS);
        ret = bh1750_get_data(bh1750, &sensor.lux);

        if (ret == ESP_OK) {
            //printf("bh1750 val(continuously mode): %f\n", sensor.lux);
        } else {
            //printf("No ack, sensor not connected...\n");
        }
    }
}

void ccs811_test_init()
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    i2c_bus = i2c_bus_create(I2C_MASTER_NUM, &conf);
    ccs811 = ccs811_create(i2c_bus, CCS811_I2C_ADDRESS_1);
    ESP_LOGI("CCS811:", "ccs811_init:%d", ccs811_init(ccs811));
}

void ccs811_test_deinit()
{
    bme280_delete(&ccs811);
    i2c_bus_delete(&i2c_bus);
}

void ccs811_test_getdata()
{
    uint16_t tvoc;
    uint16_t eco2;
    TickType_t last_wakeup = xTaskGetTickCount();
    while(1)
    {
        // get enviromental data from another sensor and set them
        // ccs811_set_enviromental_data
        if(!ccs811_get_results(ccs811, &tvoc, &eco2, 0, 0))
        {
            printf("CCS811 sensor: TVOC: %d ppb, eCO2 %d ppm\n", tvoc, eco2);
        }
        //ccs811_init(ccs811);
        vTaskDelay(1000 / portTICK_RATE_MS);
    }
}


void bme280_test_init()
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    i2c_bus = i2c_bus_create(I2C_MASTER_NUM, &conf);
    bme280 = bme280_create(i2c_bus, BME280_I2C_ADDRESS_DEFAULT);
    ESP_LOGI("BME280:", "bme280_default_init:%d", bme280_default_init(bme280));
}

void bme280_test_getdata()
{
    int cnt = 10;
    while (1) {
        float temperature = 0.0, humidity = 0.0, pressure = 0.0;
        if (ESP_OK == bme280_read_temperature(bme280, &temperature))
        ESP_LOGI("BME280", "temperature:%f ", temperature);
        vTaskDelay(300 / portTICK_RATE_MS);
        if (ESP_OK == bme280_read_humidity(bme280, &humidity))
        ESP_LOGI("BME280", "humidity:%f ", humidity);
        vTaskDelay(300 / portTICK_RATE_MS);
        if (ESP_OK == bme280_read_pressure(bme280, &pressure))
        ESP_LOGI("BME280", "pressure:%f\n", pressure);
        vTaskDelay(300 / portTICK_RATE_MS);
    }
}



// #define I2C_MASTER_SCL_IO           22          /*!< gpio number for I2C master clock */
// #define I2C_MASTER_SDA_IO           21          /*!< gpio number for I2C master data  */
// #define I2C_MASTER_NUM              I2C_NUM_1   /*!< I2C port number for master dev */
// #define I2C_MASTER_TX_BUF_DISABLE   0           /*!< I2C master do not need buffer */
// #define I2C_MASTER_RX_BUF_DISABLE   0           /*!< I2C master do not need buffer */
// #define I2C_MASTER_FREQ_HZ          100000      /*!< I2C master clock frequency */

// static i2c_bus_handle_t i2c_bus = NULL;
// static bh1750_handle_t bh1750 = NULL;

// void bh1750_test_init()
// {
//     i2c_config_t conf = {
//         .mode = I2C_MODE_MASTER,
//         .sda_io_num = I2C_MASTER_SDA_IO,
//         .sda_pullup_en = GPIO_PULLUP_ENABLE,
//         .scl_io_num = I2C_MASTER_SCL_IO,
//         .scl_pullup_en = GPIO_PULLUP_ENABLE,
//         .master.clk_speed = I2C_MASTER_FREQ_HZ,
//     };
//     i2c_bus = i2c_bus_create(I2C_MASTER_NUM, &conf);
//     bh1750 = bh1750_create(i2c_bus, BH1750_I2C_ADDRESS_DEFAULT);
// }

// void bh1750_test_deinit()
// {
//     bh1750_delete(&bh1750);
//     i2c_bus_delete(&i2c_bus);
// }

// void bh1750_test_get_data()
// {
//     int ret;
//     bh1750_cmd_measure_t cmd_measure;
//     float bh1750_data;
//     int cnt = 10;

//     while (1) {
//         bh1750_power_on(bh1750);
//         cmd_measure = BH1750_ONETIME_4LX_RES;
//         bh1750_set_measure_mode(bh1750, cmd_measure);
//         vTaskDelay(30 / portTICK_RATE_MS);
//         ret = bh1750_get_data(bh1750, &bh1750_data);

//         if (ret == ESP_OK) {
//             printf("bh1750 val(one time mode): %f\n", bh1750_data);
//         } else {
//             printf("No ack, sensor not connected...\n");
//         }

//         cmd_measure = BH1750_CONTINUE_4LX_RES;
//         bh1750_set_measure_mode(bh1750, cmd_measure);
//         vTaskDelay(30 / portTICK_RATE_MS);
//         ret = bh1750_get_data(bh1750, &bh1750_data);

//         if (ret == ESP_OK) {
//             printf("bh1750 val(continuously mode): %f\n", bh1750_data);
//         } else {
//             printf("No ack, sensor not connected...\n");
//         }

//         vTaskDelay(1000 / portTICK_RATE_MS);
//     }
// }

