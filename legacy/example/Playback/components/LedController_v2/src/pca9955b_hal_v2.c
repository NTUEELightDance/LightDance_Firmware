#include "pca9955b_hal_v2.h"

#include "string.h"

#include "esp_check.h"
#include "esp_log.h"

#define PCA9955B_PWM0_ADDR 0x08  // Address of PWM0 register
#define PCA9955B_AUTO_INC 0x80   // Auto-Increment for all registers
#define PCA9955B_IREFALL_ADDR 0x45

static const char* TAG = "PCA9955B";

uint8_t IREF_cmd[2] = {PCA9955B_IREFALL_ADDR, 0xff}; /*!< 2-byte IREF reset command to send over I2C */

esp_err_t i2c_bus_init(gpio_num_t i2c_gpio_sda, gpio_num_t i2c_gpio_scl, i2c_master_bus_handle_t* ret_i2c_bus_handle) {
    esp_err_t ret = ESP_OK;

    // 1. Input Validation
    ESP_RETURN_ON_FALSE(ret_i2c_bus_handle, ESP_ERR_INVALID_ARG, TAG, "Return handle pointer is NULL");

    // Check if pins are distinct
    ESP_RETURN_ON_FALSE(i2c_gpio_sda != i2c_gpio_scl, ESP_ERR_INVALID_ARG, TAG, "SDA and SCL cannot be the same pin");

    *ret_i2c_bus_handle = NULL; /*!< Clear output handle before initialization */

    // 2. Configuration
    i2c_master_bus_config_t i2c_bus_config = {
        .i2c_port = I2C_NUM_0,                /*!< Use I2C port 0 */
        .sda_io_num = i2c_gpio_sda,           /*!< SDA GPIO pin */
        .scl_io_num = i2c_gpio_scl,           /*!< SCL GPIO pin */
        .clk_source = I2C_CLK_SRC_DEFAULT,    /*!< Select default clock source */
        .glitch_ignore_cnt = 7,               /*!< Glitch filter (typical value) */
        .flags.enable_internal_pullup = true, /*!< Enable internal weak pull-ups */
    };

    // 3. Install Driver
    ret = i2c_new_master_bus(&i2c_bus_config, ret_i2c_bus_handle);

    if(ret != ESP_OK) {
        *ret_i2c_bus_handle = NULL;
        ESP_LOGE(TAG, "Failed to create I2C bus: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "I2C Bus initialized on SDA:%d SCL:%d", i2c_gpio_sda, i2c_gpio_scl);
    return ESP_OK;
}

esp_err_t pca9955b_init(pca9955b_dev_t* pca9955b, uint8_t i2c_addr, i2c_master_bus_handle_t i2c_bus_handle) {
    esp_err_t ret = ESP_OK;

    // 1. Input Validation
    ESP_RETURN_ON_FALSE(i2c_bus_handle, ESP_ERR_INVALID_ARG, TAG, "I2C bus handle is NULL");
    ESP_RETURN_ON_FALSE(pca9955b, ESP_ERR_INVALID_ARG, TAG, "pca9955b is NULL");
    ESP_RETURN_ON_FALSE(i2c_addr < 0x80, ESP_ERR_INVALID_ARG, TAG, "Invalid I2C address");

    memset(pca9955b, 0, sizeof(*pca9955b));

    pca9955b->i2c_addr = i2c_addr;
    pca9955b->need_reset_IREF = true;

    pca9955b->buffer.command_byte = PCA9955B_PWM0_ADDR | PCA9955B_AUTO_INC;
    memset(pca9955b->buffer.data, 0, sizeof(pca9955b->buffer.data));

    i2c_device_config_t i2c_dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7, /*!< 7-bit address mode */
        .device_address = i2c_addr,            /*!< Target device address */
        .scl_speed_hz = I2C_FREQ,              /*!< Bus clock frequency */
        .flags.disable_ack_check = false,      // We want to ensure device is connected
    };

    ESP_GOTO_ON_ERROR(i2c_master_bus_add_device(i2c_bus_handle, &i2c_dev_config, &pca9955b->i2c_dev_handle), err, TAG, "Failed to add I2C device");

    ESP_GOTO_ON_ERROR(
        i2c_master_transmit(pca9955b->i2c_dev_handle, IREF_cmd, sizeof(IREF_cmd), I2C_TIMEOUT_MS), err_dev, TAG, "Failed to set default IREF");
    pca9955b->need_reset_IREF = false;

    // 2. Clear LEDs (Set Black)
    ESP_GOTO_ON_ERROR(i2c_master_transmit(pca9955b->i2c_dev_handle, (uint8_t*)&pca9955b->buffer, sizeof(pca9955b_buffer_t), I2C_TIMEOUT_MS),
                      err_dev,
                      TAG,
                      "Failed to clear LEDs (Black)");

    ESP_LOGI(TAG, "Device initialized at address 0x%02x", i2c_addr);
    return ESP_OK;

err_dev:
    // If hardware init failed, remove the device from the bus to prevent leaks
    if(pca9955b->i2c_dev_handle) {
        i2c_master_bus_rm_device(pca9955b->i2c_dev_handle);
        pca9955b->i2c_dev_handle = NULL;
    }
err:
    // Free the allocated memory
    return ret;
}

esp_err_t pca9955b_set_pixel(pca9955b_dev_t* pca9955b, uint8_t pixel_idx, uint8_t red, uint8_t green, uint8_t blue) {
    // 1. Validate Input
    ESP_RETURN_ON_FALSE(pca9955b, ESP_ERR_INVALID_ARG, TAG, "Handle is NULL");
    ESP_RETURN_ON_FALSE(pixel_idx < 5, ESP_ERR_INVALID_ARG, TAG, "Pixel index out of range (0-4)");

    // 2. Write to Shadow Buffer
    // Assuming RGB order. Adjust indices if your hardware is GRB.
    pca9955b->buffer.ch[pixel_idx][0] = red;   /*!< Red channel */
    pca9955b->buffer.ch[pixel_idx][1] = green; /*!< Green channel */
    pca9955b->buffer.ch[pixel_idx][2] = blue;  /*!< Blue channel */

    return ESP_OK;
}

esp_err_t pca9955b_write(pca9955b_dev_t* pca9955b, const uint8_t* _buffer) {
    // 1. Validate Input
    ESP_RETURN_ON_FALSE(pca9955b, ESP_ERR_INVALID_ARG, TAG, "Handle is NULL");
    ESP_RETURN_ON_FALSE(_buffer, ESP_ERR_INVALID_ARG, TAG, "Input buffer is NULL");

    // 2. Bulk Copy Logic
    // Copy 15 bytes (5 RGB LEDs x 3 colors) directly into the shadow buffer
    memcpy(pca9955b->buffer.data, _buffer, 15 * sizeof(uint8_t));

    return ESP_OK;
}

esp_err_t pca9955b_show(pca9955b_dev_t* pca9955b) {
    esp_err_t ret = ESP_OK;

    // 1. Input Validation
    ESP_RETURN_ON_FALSE(pca9955b, ESP_ERR_INVALID_ARG, TAG, "Handle is NULL");

    // 2. IREF Restoration Logic (Recover from previous failure)
    if(pca9955b->need_reset_IREF) {
        ret = i2c_master_transmit(pca9955b->i2c_dev_handle, IREF_cmd, sizeof(IREF_cmd), I2C_TIMEOUT_MS);

        if(ret == ESP_OK) {
            pca9955b->need_reset_IREF = false; /*!< IREF reset completed */
            ESP_LOGI(TAG, "PCA9955B IREF recovered");
        } else {
            // If IREF fails, we can't show colors properly anyway.
            ESP_LOGW(TAG, "Failed to restore IREF: %s", esp_err_to_name(ret));
            return ret;
        }
    }

    // 3. Transmit Buffer (Burst Write)
    // Send 16 bytes: Command Byte (PWM0 + AI) + 15 Color Bytes
    ret = i2c_master_transmit(pca9955b->i2c_dev_handle,
                              (uint8_t*)&pca9955b->buffer,
                              sizeof(pca9955b_buffer_t),  // Safer than hardcoding '16'
                              I2C_TIMEOUT_MS);

    if(ret != ESP_OK) {
        // 4. Error Handling & Recovery Prep
        // If transmission failed, assume device might have reset or disconnected.
        // Mark IREF to be re-sent next time we try to show.
        pca9955b->need_reset_IREF = true;
        ESP_LOGE(TAG, "I2C Transmit failed: %s", esp_err_to_name(ret));
        return ret;
    }

    return ESP_OK;
}

esp_err_t pca9955b_del(pca9955b_dev_t* pca9955b) {
    if(pca9955b == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t i2c_addr = pca9955b->i2c_addr;
    // 1. Best-effort: turn off LEDs
    if(pca9955b->i2c_dev_handle) {
        memset(pca9955b->buffer.data, 0, sizeof(pca9955b->buffer.data));

        // Direct transmit: do NOT call higher-level show()
        i2c_master_transmit(pca9955b->i2c_dev_handle, (uint8_t*)&pca9955b->buffer, sizeof(pca9955b_buffer_t), I2C_TIMEOUT_MS);
    }

    // 2. Remove device from I2C bus
    if(pca9955b->i2c_dev_handle) {
        esp_err_t err = i2c_master_bus_rm_device(pca9955b->i2c_dev_handle);
        if(err != ESP_OK) {
            ESP_LOGW(TAG, "Failed to remove I2C device (0x%02x): %s", i2c_addr, esp_err_to_name(err));
        }
        pca9955b->i2c_dev_handle = NULL;
    }

    ESP_LOGI(TAG, "PCA9955B at address 0x%02x deinitialized", i2c_addr);
    return ESP_OK;
}

esp_err_t pca9955b_fill(pca9955b_dev_t* pca9955b, uint8_t red, uint8_t green, uint8_t blue) {
    // 1. Input Validation
    ESP_RETURN_ON_FALSE(pca9955b, ESP_ERR_INVALID_ARG, TAG, "Handle is NULL");

    // 2. Loop through all 5 LEDs and set color
    for(int i = 0; i < 5; i++) {
        pca9955b->buffer.ch[i][0] = red;
        pca9955b->buffer.ch[i][1] = green;
        pca9955b->buffer.ch[i][2] = blue;
    }

    return ESP_OK;
}