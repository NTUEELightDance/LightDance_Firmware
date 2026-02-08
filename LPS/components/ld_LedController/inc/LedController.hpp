#pragma once

#include <cstddef>
#include <cstdint>

#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"

#include "config.h"
#include "frame.h"
#include "ld_board.h"

#include "pca9955b.hpp"
#include "ws2812b_tx.hpp"

namespace ld {

static inline void grb_zero(grb8_t& p) {
    p.r = 0;
    p.g = 0;
    p.b = 0;
}

class LedController {
  public:
    LedController() = default;
    ~LedController() {
        deinit();
    };

    LedController(const LedController&) = delete;
    LedController& operator=(const LedController&) = delete;

    esp_err_t init(ld_channel_layout_t& layout) {
        if(inited_) {
            deinit();  // or return ESP_ERR_INVALID_STATE;
        }

        layout_ = layout;

        for(int i = 0; i < PCA9955B_NUM; i++) {
            pca_enabled_[i] = false;
        }
        for(int i = 0; i < WS2812B_NUM; i++) {
            ws_enabled_[i] = false;
        }

        for(int pix = 0; pix < PCA9955B_CH_NUM; pix++) {
            if(layout_.pca9955b_pixel_counts[pix]) {
                const int dev = pix / PCA9955B_RGB_PER_IC;
                if(dev < PCA9955B_NUM) {
                    pca_enabled_[dev] = true;
                }
            }
        }

        for(int i = 0; i < WS2812B_NUM; ++i) {
            if(layout_.ws2812b_pixel_counts[i]) {
                ws_enabled_[i] = true;
            }
        }

        esp_err_t ret = ESP_OK;

        ld_i2c_bus_config_t bus_cfg = ld_i2c_bus_config_default();
        ESP_GOTO_ON_ERROR(ld_i2c_bus_init(&bus_cfg, &bus_), fail, TAG, "i2c bus init failed");

        for(int i = 0; i < PCA9955B_NUM; ++i) {
            if(!pca_enabled_[i]) {
                continue;
            }

            pca_[i].init(bus_, BOARD_HW_CONFIG.i2c_addrs[i]);
        }

        for(int i = 0; i < WS2812B_NUM; ++i) {
            if(!ws_enabled_[i]) {
                continue;
            }

            ws_[i].init(BOARD_HW_CONFIG.rmt_pins[i]);
        }

        inited_ = true;
        return ESP_OK;

    fail:
        deinit();
        return ret;
    }

    esp_err_t deinit() {
        if(!inited_)
            return ESP_OK;

        for(int i = 0; i < PCA9955B_NUM; ++i) {
            pca_[i].deinit();
            pca_enabled_[i] = false;
        }
        for(int i = 0; i < WS2812B_NUM; ++i) {
            ws_[i].deinit();
            ws_enabled_[i] = false;
        }

        ld_i2c_bus_deinit(bus_);
        inited_ = false;

        return ESP_OK;
    }

    esp_err_t show(const ld_frame_data_t& frame_data) {
        ESP_RETURN_ON_FALSE(inited_, ESP_ERR_INVALID_STATE, TAG, "not initialized");

        const int half = (WS2812B_NUM + 1) / 2;  // first half gets the extra if odd

        auto push_ws_range = [&](int begin, int end) -> esp_err_t {
            for(int i = begin; i < end; ++i) {
                if(!ws_enabled_[i])
                    continue;

                const uint16_t n = layout_.ws2812b_pixel_counts[i];
                ESP_RETURN_ON_FALSE(n <= WS2812B_MAX_PIXEL_NUM, ESP_ERR_INVALID_ARG, TAG, "ws count overflow strip=%d n=%u", i, (unsigned)n);

                ESP_RETURN_ON_ERROR(ws_[i].set_grb_n(frame_data.ws_pixels[i], n), TAG, "ws transmit failed strip=%d", i);
            }
            return ESP_OK;
        };

        auto wait_ws_range = [&](int begin, int end) -> esp_err_t {
            for(int i = begin; i < end; ++i) {
                if(!ws_enabled_[i])
                    continue;

                ESP_RETURN_ON_ERROR(ws_[i].wait_done(), TAG, "ws wait failed strip=%d", i);
            }
            return ESP_OK;
        };

        auto push_pca_all = [&]() -> esp_err_t {
            for(int dev = 0; dev < PCA9955B_NUM; ++dev) {
                if(!pca_enabled_[dev])
                    continue;

                const int base = dev * PCA9955B_RGB_PER_IC;

                grb8_t tmp[PCA9955B_RGB_PER_IC];
                for(int j = 0; j < PCA9955B_RGB_PER_IC; ++j) {
                    const int pix = base + j;

                    if(pix < PCA9955B_CH_NUM && layout_.pca9955b_pixel_counts[pix]) {
                        tmp[j] = frame_data.pca_pixels[pix];
                    } else {
                        grb_zero(tmp[j]);
                    }
                }

                ESP_RETURN_ON_ERROR(pca_[dev].set_grb_n(tmp, PCA9955B_RGB_PER_IC, 0), TAG, "pca write failed dev=%d", dev);
            }
            return ESP_OK;
        };

        // Phase A: first half WS -> PCA -> wait first half WS
        ESP_RETURN_ON_ERROR(push_ws_range(0, half), TAG, "push ws first half failed");
        ESP_RETURN_ON_ERROR(push_pca_all(), TAG, "push pca (A) failed");
        ESP_RETURN_ON_ERROR(wait_ws_range(0, half), TAG, "wait ws first half failed");

        // Phase B: second half WS -> PCA -> wait second half WS
        ESP_RETURN_ON_ERROR(push_ws_range(half, WS2812B_NUM), TAG, "push ws second half failed");
        ESP_RETURN_ON_ERROR(push_pca_all(), TAG, "push pca (B) failed");
        ESP_RETURN_ON_ERROR(wait_ws_range(half, WS2812B_NUM), TAG, "wait ws second half failed");

        return ESP_OK;
    }

  private:
    static constexpr const char* TAG = "LedController";

    bool inited_ = false;

    ld_channel_layout_t layout_;

    bool pca_enabled_[PCA9955B_NUM] = {};
    bool ws_enabled_[WS2812B_NUM] = {};

    ld_i2c_bus_handle_t bus_;
    ld::Pca9955b pca_[PCA9955B_NUM];
    ld::Ws2812bTx ws_[WS2812B_NUM];
};

}  // namespace ld