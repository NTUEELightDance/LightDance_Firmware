#pragma once

#include <cstddef>
#include <cstdint>
#include <utility>

#include "driver/gpio.h"
#include "esp_err.h"

extern "C" {
#include "ld_ws2812b_tx.h"
}

namespace ld {

class Ws2812bTx {
  public:
    Ws2812bTx() = default;

    ~Ws2812bTx() {
        deinit();
    }

    Ws2812bTx(const Ws2812bTx&) = delete;
    Ws2812bTx& operator=(const Ws2812bTx&) = delete;

    esp_err_t init(const gpio_num_t gpio) {
        ld_ws2812b_tx_config_t cfg = ld_ws2812b_tx_config_t_default();
        cfg.gpio_num = gpio;
        return init(cfg);
    }

    esp_err_t init(const ld_ws2812b_tx_config_t& cfg) {
        ld_ws2812b_tx_handle_t tmp = nullptr;
        esp_err_t ret = ld_ws2812b_tx_init(&cfg, &tmp);
        if(ret != ESP_OK)
            return ret;

        deinit();
        h_ = tmp;
        return ESP_OK;
    }

    void deinit() {
        if(h_) {
            (void)ld_ws2812b_tx_deinit(h_);
            h_ = nullptr;
        }
    }

    bool is_valid() const {
        return h_ != nullptr;
    }

    explicit operator bool() const {
        return is_valid();
    }

    ld_ws2812b_tx_handle_t handle() const {
        return h_;
    }

    esp_err_t transmit(const uint8_t* bytes, size_t len) {
        return ld_ws2812b_tx_transmit_bytes(h_, bytes, len);
    }

    esp_err_t wait_done(uint32_t timeout_ms = LD_WS2812B_TX_DEFAULT_WAIT_DONE_TIMEOUT_MS) {
        return ld_ws2812b_tx_wait_done(h_, timeout_ms);
    }

    bool busy() const {
        return ld_ws2812b_tx_busy(h_);
    }

    gpio_num_t gpio() const {
        return ld_ws2812b_tx_gpio(h_);
    }

    esp_err_t set_grb_n(const grb8_t* px, size_t pixel_n) {
        ESP_RETURN_ON_FALSE(px != nullptr, ESP_ERR_INVALID_ARG, TAG, "px is null");
        ESP_RETURN_ON_FALSE(h_ != nullptr, ESP_ERR_INVALID_STATE, TAG, "not initialized");

        // ws2812b expects a contiguous stream; do not split into multiple transmits.
        ESP_RETURN_ON_FALSE(sizeof(grb8_t) == 3, ESP_ERR_INVALID_SIZE, TAG, "grb8_t must be 3 bytes");

        const size_t len = pixel_n * 3;
        return ld_ws2812b_tx_transmit_bytes(h_, reinterpret_cast<const uint8_t*>(px), len);
    }

  private:
    static constexpr const char* TAG = "Ws2812bTx";

    ld_ws2812b_tx_handle_t h_ = nullptr;
};

}  // namespace ld