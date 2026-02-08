#pragma once
#include "esp_err.h"
#include "ld_i2c_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LD_I2C_SCL_SPEED_HZ_DEFAULT (400000U) /*!< 400 kHz */
#define LD_I2C_TIMEOUT_MS_DEFAULT (5U)        /*!< transaction timeout */
#define LD_I2C_ADDR_7BIT_MAX (0x7FU)

#define LD_I2C_REG_WRITE_MAX_LEN (32U)

typedef struct {
    uint8_t addr_7bit;
    uint32_t scl_speed_hz;  // e.g. 400k
    uint32_t timeout_ms;    // bus transaction timeout
} ld_i2c_dev_config_t;

static inline ld_i2c_dev_config_t ld_i2c_dev_config_default(uint8_t addr_7bit) {
    return (ld_i2c_dev_config_t){
        .addr_7bit = addr_7bit,
        .scl_speed_hz = LD_I2C_SCL_SPEED_HZ_DEFAULT,
        .timeout_ms = LD_I2C_TIMEOUT_MS_DEFAULT,
    };
}

esp_err_t ld_i2c_dev_add(ld_i2c_bus_handle_t bus, const ld_i2c_dev_config_t* cfg, ld_i2c_dev_handle_t* out);
esp_err_t ld_i2c_dev_del(ld_i2c_dev_handle_t dev);

// basic ops
esp_err_t ld_i2c_write(ld_i2c_dev_handle_t dev, const uint8_t* data, size_t len);
esp_err_t ld_i2c_read(ld_i2c_dev_handle_t dev, uint8_t* data, size_t len);
esp_err_t ld_i2c_write_read(ld_i2c_dev_handle_t dev, const uint8_t* w, size_t wlen, uint8_t* r, size_t rlen);

// common register helpers
esp_err_t ld_i2c_reg_write_u8(ld_i2c_dev_handle_t dev, uint8_t reg, uint8_t val);
esp_err_t ld_i2c_reg_read_u8(ld_i2c_dev_handle_t dev, uint8_t reg, uint8_t* val);
esp_err_t ld_i2c_reg_write(ld_i2c_dev_handle_t dev, uint8_t reg, const uint8_t* data, size_t len);
esp_err_t ld_i2c_reg_read(ld_i2c_dev_handle_t dev, uint8_t reg, uint8_t* data, size_t len);

#ifdef __cplusplus
}
#endif