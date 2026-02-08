#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_check.h"
#include "esp_log.h"

#include "ld_i2c_bus.h"
#include "ld_i2c_dev.h"

static const char* TAG = "ld_i2c_basic";

#define PCA9955B_I2C_ADDR_7BIT (0x1F)

#ifndef PCA9955B_REG_PWM0
#define PCA9955B_REG_PWM0 (0x08)  // PWM0 register offset
#endif

#ifndef PCA9955B_FLAG_AUTO_INC
#define PCA9955B_FLAG_AUTO_INC (0x80)  // Auto-increment for all registers
#endif

#ifndef PCA9955B_REG_IREFALL
#define PCA9955B_REG_IREFALL (0x45)  // IREFALL register offset
#endif

void app_main(void) {
    esp_err_t ret = ESP_OK;

    // 1) Bus init
    ld_i2c_bus_handle_t bus = NULL;
    ld_i2c_bus_config_t bus_cfg = ld_i2c_bus_config_default();

    ret = ld_i2c_bus_init(&bus_cfg, &bus);
    ESP_RETURN_ON_ERROR(ret, TAG, "ld_i2c_bus_init failed");

    // 2) Device add
    ld_i2c_dev_handle_t dev = NULL;
    ld_i2c_dev_config_t dev_cfg = ld_i2c_dev_config_default(PCA9955B_I2C_ADDR_7BIT);

    ret = ld_i2c_dev_add(bus, &dev_cfg, &dev);
    if(ret != ESP_OK) {
        ESP_LOGE(TAG, "ld_i2c_dev_add failed: %s", esp_err_to_name(ret));
        goto cleanup_bus;
    }

    // 3) Write a single register
    ret = ld_i2c_reg_write_u8(dev, PCA9955B_REG_IREFALL, 0xFF);
    if(ret != ESP_OK) {
        ESP_LOGE(TAG, "ld_i2c_reg_write_u8 failed: %s", esp_err_to_name(ret));
        goto cleanup_dev;
    }

    // 4) Write PWM block (15 bytes)
    uint8_t data[EX_PWM_LEN];
    memset(data, EX_ON_VAL, sizeof(data));

    ret = ld_i2c_reg_write(dev, PCA9955B_REG_PWM0 | PCA9955B_FLAG_AUTO_INC, data, sizeof(data));
    if(ret != ESP_OK) {
        ESP_LOGE(TAG, "ld_i2c_reg_write (ON) failed: %s", esp_err_to_name(ret));
        goto cleanup_dev;
    }

    vTaskDelay(pdMS_TO_TICKS(EX_DELAY_MS));

    // 5) Turn off
    memset(data, 0, sizeof(data));
    ret = ld_i2c_reg_write(dev, PCA9955B_REG_PWM0 | PCA9955B_FLAG_AUTO_INC, data, sizeof(data));
    if(ret != ESP_OK) {
        ESP_LOGE(TAG, "ld_i2c_reg_write (OFF) failed: %s", esp_err_to_name(ret));
        goto cleanup_dev;
    }

cleanup_dev:
    if(dev) {
        (void)ld_i2c_dev_del(dev);
        dev = NULL;
    }

cleanup_bus:
    if(bus) {
        (void)ld_i2c_bus_deinit(bus);
        bus = NULL;
    }

    ESP_LOGI(TAG, "done");
}
