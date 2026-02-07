#include "ld_i2c_dev.h"

#include <stdlib.h>
#include <string.h>

#include "esp_check.h"
#include "esp_log.h"

#include "ld_i2c_bus.h"

static const char* TAG = "ld_i2c_dev";

struct ld_i2c_dev_t {
    ld_i2c_dev_config_t cfg;
    i2c_master_dev_handle_t dev;
    ld_i2c_bus_handle_t bus_owner;
};

esp_err_t ld_i2c_dev_add(ld_i2c_bus_handle_t bus, const ld_i2c_dev_config_t* cfg, ld_i2c_dev_handle_t* out) {
    esp_err_t ret = ESP_OK;

    ESP_RETURN_ON_FALSE(bus, ESP_ERR_INVALID_ARG, TAG, "bus is NULL");
    ESP_RETURN_ON_FALSE(cfg, ESP_ERR_INVALID_ARG, TAG, "cfg is NULL");
    ESP_RETURN_ON_FALSE(out, ESP_ERR_INVALID_ARG, TAG, "out is NULL");

    ESP_RETURN_ON_FALSE(cfg->addr_7bit <= LD_I2C_ADDR_7BIT_MAX, ESP_ERR_INVALID_ARG, TAG, "addr out of range");
    ESP_RETURN_ON_FALSE(cfg->addr_7bit >= 0x03 && cfg->addr_7bit <= 0x77, ESP_ERR_INVALID_ARG, TAG, "addr reserved");
    ESP_RETURN_ON_FALSE(cfg->scl_speed_hz > 0, ESP_ERR_INVALID_ARG, TAG, "scl_speed_hz=0");
    ESP_RETURN_ON_FALSE(cfg->timeout_ms > 0, ESP_ERR_INVALID_ARG, TAG, "timeout_ms=0");

    *out = NULL;

    i2c_master_bus_handle_t bus_handle = ld_i2c_bus_native_handle(bus);
    ESP_RETURN_ON_FALSE(bus_handle, ESP_ERR_INVALID_STATE, TAG, "bus not initialized");

    ld_i2c_dev_t* h = (ld_i2c_dev_t*)calloc(1, sizeof(ld_i2c_dev_t));
    ESP_RETURN_ON_FALSE(h, ESP_ERR_NO_MEM, TAG, "no mem for dev");

    h->cfg = *cfg;
    h->dev = NULL;
    h->bus_owner = bus;

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = cfg->addr_7bit,
        .scl_speed_hz = cfg->scl_speed_hz,
    };

    ret = i2c_master_bus_add_device(bus_handle, &dev_cfg, &h->dev);
    if(ret != ESP_OK) {
        ESP_LOGE(TAG, "i2c_master_bus_add_device failed: %s", esp_err_to_name(ret));
        free(h);
        return ret;
    }

    *out = h;
    return ESP_OK;
}

esp_err_t ld_i2c_dev_del(ld_i2c_dev_handle_t dev) {
    ESP_RETURN_ON_FALSE(dev, ESP_ERR_INVALID_ARG, TAG, "dev is NULL");

    if(dev->dev) {
        esp_err_t ret = i2c_master_bus_rm_device(dev->dev);
        ESP_RETURN_ON_ERROR(ret, TAG, "i2c_master_bus_rm_device failed");
        dev->dev = NULL;
    }

    free(dev);
    return ESP_OK;
}

esp_err_t ld_i2c_write(ld_i2c_dev_handle_t dev, const uint8_t* data, size_t len) {
    ESP_RETURN_ON_FALSE(dev, ESP_ERR_INVALID_ARG, TAG, "dev is NULL");
    ESP_RETURN_ON_FALSE(dev->dev, ESP_ERR_INVALID_STATE, TAG, "native dev handle is NULL");
    ESP_RETURN_ON_FALSE(data, ESP_ERR_INVALID_ARG, TAG, "data is NULL");
    ESP_RETURN_ON_FALSE(len > 0, ESP_ERR_INVALID_ARG, TAG, "len must be > 0");

    int tmo = (dev->cfg.timeout_ms > (uint32_t)INT32_MAX) ? INT32_MAX : (int)dev->cfg.timeout_ms;

    esp_err_t ret = i2c_master_transmit(dev->dev, data, len, tmo);
    ESP_RETURN_ON_ERROR(ret, TAG, "i2c_master_transmit failed");
    return ESP_OK;
}

esp_err_t ld_i2c_read(ld_i2c_dev_handle_t dev, uint8_t* data, size_t len) {
    ESP_RETURN_ON_FALSE(dev, ESP_ERR_INVALID_ARG, TAG, "dev is NULL");
    ESP_RETURN_ON_FALSE(dev->dev, ESP_ERR_INVALID_STATE, TAG, "native dev handle is NULL");
    ESP_RETURN_ON_FALSE(data, ESP_ERR_INVALID_ARG, TAG, "data is NULL");
    ESP_RETURN_ON_FALSE(len > 0, ESP_ERR_INVALID_ARG, TAG, "len must be > 0");

    int tmo = (dev->cfg.timeout_ms > (uint32_t)INT32_MAX) ? INT32_MAX : (int)dev->cfg.timeout_ms;

    esp_err_t ret = i2c_master_receive(dev->dev, data, len, tmo);
    ESP_RETURN_ON_ERROR(ret, TAG, "i2c_master_receive failed");
    return ESP_OK;
}

esp_err_t ld_i2c_write_read(ld_i2c_dev_handle_t dev, const uint8_t* w, size_t wlen, uint8_t* r, size_t rlen) {
    ESP_RETURN_ON_FALSE(dev, ESP_ERR_INVALID_ARG, TAG, "dev is NULL");
    ESP_RETURN_ON_FALSE(dev->dev, ESP_ERR_INVALID_STATE, TAG, "native dev handle is NULL");

    ESP_RETURN_ON_FALSE(w, ESP_ERR_INVALID_ARG, TAG, "w is NULL");
    ESP_RETURN_ON_FALSE(r, ESP_ERR_INVALID_ARG, TAG, "r is NULL");
    ESP_RETURN_ON_FALSE(wlen > 0, ESP_ERR_INVALID_ARG, TAG, "wlen must be > 0");
    ESP_RETURN_ON_FALSE(rlen > 0, ESP_ERR_INVALID_ARG, TAG, "rlen must be > 0");

    int tmo = (dev->cfg.timeout_ms > (uint32_t)INT32_MAX) ? INT32_MAX : (int)dev->cfg.timeout_ms;

    esp_err_t ret = i2c_master_transmit_receive(dev->dev, w, wlen, r, rlen, tmo);
    ESP_RETURN_ON_ERROR(ret, TAG, "i2c_master_transmit_receive failed");
    return ESP_OK;
}

esp_err_t ld_i2c_reg_write_u8(ld_i2c_dev_handle_t dev, uint8_t reg, uint8_t val) {
    ESP_RETURN_ON_FALSE(dev, ESP_ERR_INVALID_ARG, TAG, "dev is NULL");

    uint8_t buf[2] = {reg, val};
    return ld_i2c_write(dev, buf, sizeof(buf));
}

esp_err_t ld_i2c_reg_read_u8(ld_i2c_dev_handle_t dev, uint8_t reg, uint8_t* val) {
    ESP_RETURN_ON_FALSE(dev, ESP_ERR_INVALID_ARG, TAG, "dev is NULL");
    ESP_RETURN_ON_FALSE(val, ESP_ERR_INVALID_ARG, TAG, "val is NULL");

    // write reg, then read 1 byte
    return ld_i2c_write_read(dev, &reg, 1, val, 1);
}

esp_err_t ld_i2c_reg_write(ld_i2c_dev_handle_t dev, uint8_t reg, const uint8_t* data, size_t len) {
    ESP_RETURN_ON_FALSE(dev, ESP_ERR_INVALID_ARG, TAG, "dev is NULL");
    ESP_RETURN_ON_FALSE(data, ESP_ERR_INVALID_ARG, TAG, "data is NULL");
    ESP_RETURN_ON_FALSE(len > 0, ESP_ERR_INVALID_ARG, TAG, "len must be > 0");
    ESP_RETURN_ON_FALSE(len <= LD_I2C_REG_WRITE_MAX_LEN, ESP_ERR_INVALID_SIZE, TAG, "len too large (max=%u)", (unsigned)LD_I2C_REG_WRITE_MAX_LEN);

    uint8_t buf[1 + LD_I2C_REG_WRITE_MAX_LEN];
    buf[0] = reg;
    memcpy(&buf[1], data, len);

    return ld_i2c_write(dev, buf, 1 + len);
}

esp_err_t ld_i2c_reg_read(ld_i2c_dev_handle_t dev, uint8_t reg, uint8_t* data, size_t len) {
    ESP_RETURN_ON_FALSE(dev, ESP_ERR_INVALID_ARG, TAG, "dev is NULL");
    ESP_RETURN_ON_FALSE(data, ESP_ERR_INVALID_ARG, TAG, "data is NULL");
    ESP_RETURN_ON_FALSE(len > 0, ESP_ERR_INVALID_ARG, TAG, "len must be > 0");

    // write reg, then read len bytes (repeated-start)
    return ld_i2c_write_read(dev, &reg, 1, data, len);
}