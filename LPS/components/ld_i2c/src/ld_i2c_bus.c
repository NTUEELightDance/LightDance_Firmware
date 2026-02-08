#include "ld_i2c_bus.h"

#include <stdlib.h>
#include <string.h>

#include "driver/i2c_master.h"
#include "esp_check.h"
#include "esp_log.h"

static const char* TAG = "ld_i2c_bus";

struct ld_i2c_bus_t {
    ld_i2c_bus_config_t cfg;
    i2c_master_bus_handle_t bus;
    bool inited;
};

ld_i2c_bus_config_t ld_i2c_bus_config_default() {
    return (ld_i2c_bus_config_t){
        .port = LD_I2C_PORT_DEFAULT,
        .sda_io = LD_I2C_SDA_GPIO_DEFAULT,
        .scl_io = LD_I2C_SCL_GPIO_DEFAULT,
        .glitch_ignore_cnt = 7,          // typical default
        .enable_internal_pullup = true,  // ok for short wires; disable if you already have strong external pullups
    };
}

esp_err_t ld_i2c_bus_init(const ld_i2c_bus_config_t* cfg, ld_i2c_bus_handle_t* out) {
    esp_err_t ret = ESP_OK;

    ESP_RETURN_ON_FALSE(cfg, ESP_ERR_INVALID_ARG, TAG, "cfg is NULL");
    ESP_RETURN_ON_FALSE(out, ESP_ERR_INVALID_ARG, TAG, "out is NULL");
    ESP_RETURN_ON_FALSE(cfg->sda_io != cfg->scl_io, ESP_ERR_INVALID_ARG, TAG, "SDA and SCL cannot be same pin");
    ESP_RETURN_ON_FALSE(cfg->sda_io >= 0, ESP_ERR_INVALID_ARG, TAG, "invalid SDA GPIO");
    ESP_RETURN_ON_FALSE(cfg->scl_io >= 0, ESP_ERR_INVALID_ARG, TAG, "invalid SCL GPIO");

    *out = NULL;

    ld_i2c_bus_t* h = (ld_i2c_bus_t*)calloc(1, sizeof(ld_i2c_bus_t));
    ESP_RETURN_ON_FALSE(h, ESP_ERR_NO_MEM, TAG, "no mem for handle");

    h->cfg = *cfg;
    h->bus = NULL;
    h->inited = false;

    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = cfg->port,
        .sda_io_num = cfg->sda_io,
        .scl_io_num = cfg->scl_io,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = cfg->glitch_ignore_cnt,
        .flags =
            {
                .enable_internal_pullup = cfg->enable_internal_pullup ? 1 : 0,
            },
    };

    ret = i2c_new_master_bus(&bus_cfg, &h->bus);
    if(ret != ESP_OK) {
        ESP_LOGE(TAG, "i2c_new_master_bus failed: %s", esp_err_to_name(ret));
        goto fail;
    }

    h->inited = true;
    *out = h;

    ESP_LOGI(TAG, "bus init ok: port=%d sda=%d scl=%d pullup=%d glitch=%u", (int)cfg->port, (int)cfg->sda_io, (int)cfg->scl_io, (int)cfg->enable_internal_pullup, (unsigned)cfg->glitch_ignore_cnt);

    return ESP_OK;

fail:
    if(h) {
        if(h->bus) {
            (void)i2c_del_master_bus(h->bus);
            h->bus = NULL;
        }
        free(h);
    }
    return ret;
}

esp_err_t ld_i2c_bus_deinit(ld_i2c_bus_handle_t h) {
    ESP_RETURN_ON_FALSE(h, ESP_ERR_INVALID_ARG, TAG, "h is NULL");

    if(h->bus) {
        esp_err_t ret = i2c_del_master_bus(h->bus);
        ESP_RETURN_ON_ERROR(ret, TAG, "i2c_del_master_bus failed");
        h->bus = NULL;
    }

    h->inited = false;
    free(h);
    return ESP_OK;
}

i2c_master_bus_handle_t ld_i2c_bus_native_handle(ld_i2c_bus_handle_t h) {
    if(!h)
        return NULL;
    return h->bus;
}

esp_err_t ld_i2c_bus_probe(ld_i2c_bus_handle_t h, uint8_t addr_7bit, uint32_t timeout_ms) {
    ESP_RETURN_ON_FALSE(h, ESP_ERR_INVALID_ARG, TAG, "h is NULL");
    ESP_RETURN_ON_FALSE(h->bus, ESP_ERR_INVALID_STATE, TAG, "bus not initialized");
    ESP_RETURN_ON_FALSE(addr_7bit <= 0x7F, ESP_ERR_INVALID_ARG, TAG, "addr out of range");

    int tmo = (timeout_ms > (uint32_t)INT32_MAX) ? INT32_MAX : (int)timeout_ms;

    esp_err_t ret = i2c_master_probe(h->bus, addr_7bit, tmo);
    return ret;
}

esp_err_t ld_i2c_bus_scan(ld_i2c_bus_handle_t h, uint8_t* out_addrs, size_t cap, size_t* out_count) {
    ESP_RETURN_ON_FALSE(h, ESP_ERR_INVALID_ARG, TAG, "h is NULL");
    ESP_RETURN_ON_FALSE(h->bus, ESP_ERR_INVALID_STATE, TAG, "bus not initialized");

    size_t found = 0;

    // 7-bit I2C address scan range typically 0x03..0x77
    for(uint8_t addr = 0x03; addr <= 0x77; ++addr) {
        esp_err_t ret = i2c_master_probe(h->bus, addr, 10 /*ms*/);
        if(ret == ESP_OK) {
            if(out_addrs && found < cap) {
                out_addrs[found] = addr;
            }
            found++;
        }
    }

    if(out_count) {
        *out_count = found;
    }

    // If caller provided buffer but it was too small, report truncation (optional policy)
    if(out_addrs && found > cap) {
        ESP_LOGW(TAG, "scan truncated: found=%u cap=%u", (unsigned)found, (unsigned)cap);
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

esp_err_t ld_i2c_bus_scan_dump(ld_i2c_bus_handle_t h, uint32_t timeout_ms) {
    ESP_RETURN_ON_FALSE(h, ESP_ERR_INVALID_ARG, TAG, "h is NULL");
    ESP_RETURN_ON_FALSE(h->bus, ESP_ERR_INVALID_STATE, TAG, "bus not initialized");

    int tmo = (timeout_ms > (uint32_t)INT32_MAX) ? INT32_MAX : (int)timeout_ms;

    // Header
    ESP_LOGI(TAG, "     0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F");

    // Common scan window 0x03..0x77
    for(uint8_t base = 0x00; base <= 0x70; base += 0x10) {
        char line[128];
        int n = 0;

        n += snprintf(line + n, sizeof(line) - n, "%02X: ", base);

        for(uint8_t off = 0; off < 0x10; ++off) {
            uint8_t addr = base + off;

            // Out of typical 7-bit address range -> print spaces
            if(addr < 0x03 || addr > 0x77) {
                n += snprintf(line + n, sizeof(line) - n, "   ");
                continue;
            }

            esp_err_t ret = i2c_master_probe(h->bus, addr, tmo);
            if(ret == ESP_OK) {
                n += snprintf(line + n, sizeof(line) - n, "%02X ", addr);
            } else {
                n += snprintf(line + n, sizeof(line) - n, "-- ");
            }
        }

        ESP_LOGI(TAG, "%s", line);
    }

    return ESP_OK;
}
