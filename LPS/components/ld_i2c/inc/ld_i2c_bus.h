#pragma once
#include "esp_err.h"
#include "ld_i2c_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LD_I2C_PORT_DEFAULT I2C_NUM_0
#define LD_I2C_SDA_GPIO_DEFAULT GPIO_NUM_21
#define LD_I2C_SCL_GPIO_DEFAULT GPIO_NUM_22

#define LD_I2C_PROBE_TIMEOUT_MS 10

typedef struct {
    i2c_port_t port;
    gpio_num_t sda_io;
    gpio_num_t scl_io;
    uint32_t glitch_ignore_cnt;
    bool enable_internal_pullup;
} ld_i2c_bus_config_t;

ld_i2c_bus_config_t ld_i2c_bus_config_default();

esp_err_t ld_i2c_bus_init(const ld_i2c_bus_config_t* cfg, ld_i2c_bus_handle_t* out);
esp_err_t ld_i2c_bus_deinit(ld_i2c_bus_handle_t h);
i2c_master_bus_handle_t ld_i2c_bus_native_handle(ld_i2c_bus_handle_t h);

// util
esp_err_t ld_i2c_bus_probe(ld_i2c_bus_handle_t h, uint8_t addr_7bit, uint32_t timeout_ms);
esp_err_t ld_i2c_bus_scan(ld_i2c_bus_handle_t h, uint8_t* out_addrs, size_t cap, size_t* out_count);
esp_err_t ld_i2c_bus_scan_dump(ld_i2c_bus_handle_t h, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif