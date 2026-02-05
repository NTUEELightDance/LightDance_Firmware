#pragma once

#include <cstddef>
#include <cstdint>

#include "esp_check.h"
#include "esp_err.h"

extern "C" {
#include "ld_i2c_bus.h"
#include "ld_i2c_dev.h"
}
namespace ld {

#ifndef PCA9955B_MODE1_ADDR
#define PCA9955B_MODE1_ADDR 0x00
#endif
#ifndef PCA9955B_MODE2_ADDR
#define PCA9955B_MODE2_ADDR 0x01
#endif
#ifndef PCA9955B_LEDOUT0_ADDR
#define PCA9955B_LEDOUT0_ADDR 0x02
#endif
#ifndef PCA9955B_GRPPWM_ADDR
#define PCA9955B_GRPPWM_ADDR 0x06
#endif
#ifndef PCA9955B_GRPFREQ_ADDR
#define PCA9955B_GRPFREQ_ADDR 0x07
#endif
#ifndef PCA9955B_PWM0_ADDR
#define PCA9955B_PWM0_ADDR 0x08
#endif
#ifndef PCA9955B_IREF0_ADDR
#define PCA9955B_IREF0_ADDR 0x22
#endif
#ifndef PCA9955B_IREFALL_ADDR
#define PCA9955B_IREFALL_ADDR 0x45
#endif
#ifndef PCA9955B_EFLAG0_ADDR
#define PCA9955B_EFLAG0_ADDR 0x46
#endif
#ifndef PCA9955B_AUTO_INC
#define PCA9955B_AUTO_INC 0x80
#endif

class Pca9955b {
  public:
    static constexpr uint8_t kChannels = 15;

    enum class LedOutMode : uint8_t {
        Off = 0b00,       // Output off (Hi-Z)
        On = 0b01,        // Output fully on (no PWM)
        Pwm = 0b10,       // Output controlled by PWMx
        PwmGroup = 0b11,  // Output controlled by PWMx and GRPPWM/GRPFREQ
    };

    Pca9955b() = default;
    ~Pca9955b() {
        deinit();
    }

    Pca9955b(const Pca9955b&) = delete;
    Pca9955b& operator=(const Pca9955b&) = delete;

    esp_err_t init(ld_i2c_bus_handle_t bus, const uint8_t addr_7bit) {
        if(addr_7bit > 0x7F)
            return ESP_ERR_INVALID_ARG;
        ld_i2c_dev_config_t cfg = ld_i2c_dev_config_default(addr_7bit);
        return init(bus, cfg);
    }

    esp_err_t init(ld_i2c_bus_handle_t bus, const ld_i2c_dev_config_t& cfg) {
        ld_i2c_dev_handle_t tmp = nullptr;
        esp_err_t ret = ld_i2c_dev_add(bus, &cfg, &tmp);
        if(ret != ESP_OK)
            return ret;

        deinit();
        dev_ = tmp;
        return ESP_OK;
    }

    void deinit() {
        if(dev_) {
            (void)ld_i2c_dev_del(dev_);
            dev_ = nullptr;
        }
    }

    bool is_valid() const {
        return dev_ != nullptr;
    }
    explicit operator bool() const {
        return is_valid();
    }

    ld_i2c_dev_handle_t handle() const {
        return dev_;
    }

    // Basic ops
    esp_err_t write(const uint8_t* data, size_t len) {
        return ld_i2c_write(dev_, data, len);
    }
    esp_err_t read(uint8_t* data, size_t len) {
        return ld_i2c_read(dev_, data, len);
    }
    esp_err_t write_read(const uint8_t* w, size_t wlen, uint8_t* r, size_t rlen) {
        return ld_i2c_write_read(dev_, w, wlen, r, rlen);
    }

    // Register helpers
    esp_err_t reg_write_u8(uint8_t reg, uint8_t val) {
        return ld_i2c_reg_write_u8(dev_, reg, val);
    }
    esp_err_t reg_read_u8(uint8_t reg, uint8_t* val) {
        return ld_i2c_reg_read_u8(dev_, reg, val);
    }
    esp_err_t reg_write(uint8_t reg, const uint8_t* data, size_t len) {
        return ld_i2c_reg_write(dev_, reg, data, len);
    }
    esp_err_t reg_read(uint8_t reg, uint8_t* data, size_t len) {
        return ld_i2c_reg_read(dev_, reg, data, len);
    }

    esp_err_t reg_write_ai(uint8_t reg, const uint8_t* data, size_t len) {
        return reg_write(static_cast<uint8_t>(reg | PCA9955B_AUTO_INC), data, len);
    }

    esp_err_t reg_read_ai(uint8_t reg, uint8_t* data, size_t len) {
        return reg_read(static_cast<uint8_t>(reg | PCA9955B_AUTO_INC), data, len);
    }

    // Convenience
    esp_err_t set_mode1(uint8_t v) {
        return reg_write_u8(PCA9955B_MODE1_ADDR, v);
    }
    esp_err_t set_mode2(uint8_t v) {
        return reg_write_u8(PCA9955B_MODE2_ADDR, v);
    }

    esp_err_t set_group_pwm(uint8_t grppwm) {
        return reg_write_u8(PCA9955B_GRPPWM_ADDR, grppwm);
    }
    esp_err_t set_group_freq(uint8_t grpfreq) {
        return reg_write_u8(PCA9955B_GRPFREQ_ADDR, grpfreq);
    }

    esp_err_t set_pwm(uint8_t ch, uint8_t pwm) {
        if(ch >= kChannels)
            return ESP_ERR_INVALID_ARG;
        return reg_write_u8(static_cast<uint8_t>(PCA9955B_PWM0_ADDR + ch), pwm);
    }

    esp_err_t set_pwm_all(const uint8_t pwm[kChannels]) {
        return reg_write_ai(PCA9955B_PWM0_ADDR, pwm, kChannels);
    }

    esp_err_t set_iref(uint8_t ch, uint8_t iref) {
        if(ch >= kChannels)
            return ESP_ERR_INVALID_ARG;
        return reg_write_u8(static_cast<uint8_t>(PCA9955B_IREF0_ADDR + ch), iref);
    }

    esp_err_t set_iref_all(const uint8_t iref[kChannels]) {
        return reg_write_ai(PCA9955B_IREF0_ADDR, iref, kChannels);
    }

    esp_err_t set_iref_all_global(uint8_t irefall) {
        return reg_write_u8(PCA9955B_IREFALL_ADDR, irefall);
    }

    esp_err_t set_ledout_mode(uint8_t ch, LedOutMode mode) {
        if(ch >= kChannels)
            return ESP_ERR_INVALID_ARG;

        uint8_t reg = static_cast<uint8_t>(PCA9955B_LEDOUT0_ADDR + (ch / 4));
        uint8_t shift = static_cast<uint8_t>((ch % 4) * 2);

        uint8_t v = 0;
        esp_err_t ret = reg_read_u8(reg, &v);
        if(ret != ESP_OK)
            return ret;

        v = static_cast<uint8_t>((v & ~(0x3u << shift)) | (static_cast<uint8_t>(mode) << shift));
        return reg_write_u8(reg, v);
    }

    esp_err_t set_ledout_all(const LedOutMode modes[kChannels]) {
        uint8_t ledout[4] = {0, 0, 0, 0};
        for(uint8_t ch = 0; ch < kChannels; ++ch) {
            uint8_t idx = static_cast<uint8_t>(ch / 4);
            uint8_t shift = static_cast<uint8_t>((ch % 4) * 2);
            ledout[idx] = static_cast<uint8_t>(ledout[idx] | (static_cast<uint8_t>(modes[ch]) << shift));
        }
        return reg_write_ai(PCA9955B_LEDOUT0_ADDR, ledout, 4);
    }

    esp_err_t read_eflags(uint8_t* data, size_t len) {
        return reg_read_ai(PCA9955B_EFLAG0_ADDR, data, len);
    }

    esp_err_t set_grb_n(const grb8_t* pixels, size_t pixel_n, uint8_t start_ch = 0) {
        ESP_RETURN_ON_FALSE(pixels != nullptr, ESP_ERR_INVALID_ARG, TAG, "px is null");

        if(pixel_n == 0)
            return ESP_OK;

        const size_t nbytes = pixel_n * 3;
        ESP_RETURN_ON_FALSE(start_ch < kChannels, ESP_ERR_INVALID_ARG, TAG, "start_ch out of range");
        ESP_RETURN_ON_FALSE(start_ch + nbytes <= kChannels, ESP_ERR_INVALID_ARG, TAG, "range overflow");

        uint8_t buf[16];
        for(size_t i = 0; i < pixel_n; ++i) {
            buf[i * 3 + 0] = pixels[i].r;  // CH = R
            buf[i * 3 + 1] = pixels[i].g;  // CH = G
            buf[i * 3 + 2] = pixels[i].b;  // CH = B
        }

        const uint8_t start_reg = static_cast<uint8_t>(PCA9955B_PWM0_ADDR + 3 * start_ch);
        const uint8_t reg_ai = static_cast<uint8_t>(start_reg | PCA9955B_AUTO_INC);

        ESP_RETURN_ON_ERROR(reg_write(reg_ai, buf, nbytes), TAG, "pwm write failed");
        return ESP_OK;
    }

  private:
    static constexpr const char* TAG = "Pca9955b";

    ld_i2c_dev_handle_t dev_ = nullptr;
};

}  // namespace ld