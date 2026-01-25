#pragma once

#include "driver/i2c_master.h"

#include "BoardConfig.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t command_byte;
    union {
        uint8_t data[15]; /*!< Raw access to 15-byte channel color payload */
        struct {
            uint8_t ch[5][3]; /*!< logical mapping: 5 LEDs x 3 channels (R, G, B) */
        };
    };
} pca9955b_buffer_t;

typedef struct {
    i2c_master_dev_handle_t i2c_dev_handle; /*!< I2C bus device handle */
    uint8_t i2c_addr;                       /*!< 7-bit I2C device address */

    pca9955b_buffer_t buffer; /*!< PWM register + LED color buffer */

    bool need_reset_IREF; /*!< Set true if IREF register needs to be reinitialized */
} pca9955b_dev_t;

esp_err_t i2c_bus_init(gpio_num_t i2c_gpio_sda, gpio_num_t i2c_gpio_scl, i2c_master_bus_handle_t* ret_i2c_bus_handle);

esp_err_t pca9955b_init(pca9955b_dev_t* pca9955b, uint8_t i2c_addr, i2c_master_bus_handle_t i2c_bus_handle);

esp_err_t pca9955b_set_pixel(pca9955b_dev_t* pca9955b, uint8_t pixel_idx, uint8_t red, uint8_t green, uint8_t blue);

esp_err_t pca9955b_show(pca9955b_dev_t* pca9955b);

esp_err_t pca9955b_del(pca9955b_dev_t* pca9955b);

esp_err_t pca9955b_write(pca9955b_dev_t* pca9955b, const uint8_t* buffer);

esp_err_t pca9955b_fill(pca9955b_dev_t* pca9955b, uint8_t red, uint8_t green, uint8_t blue);

void pca9955b_test1();
void pca9955b_test2();

#ifdef __cplusplus
}
#endif