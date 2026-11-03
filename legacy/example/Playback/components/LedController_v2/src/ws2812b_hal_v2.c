#include "ws2812b_hal_v2.h"

#include "string.h"

#include "esp_check.h"
#include "esp_log.h"

#define WS2812B_RESOLUTION 10000000
static const char* TAG = "WS2812";

// Config: One-shot transmission, idle low (reset), non-blocking
static const rmt_transmit_config_t rmt_tx_config = {
    .loop_count = 0,
    .flags =
        {
            .eot_level = 0,
            .queue_nonblocking = true,
        },
};

static esp_err_t ws2812b_init_channel(gpio_num_t gpio_num, uint16_t pixel_num, rmt_channel_handle_t* channel) {
    // Check for critical null pointer and hardware validity
    ESP_RETURN_ON_FALSE(channel, ESP_ERR_INVALID_ARG, TAG, "Channel pointer is invalid");
    ESP_RETURN_ON_FALSE(GPIO_IS_VALID_OUTPUT_GPIO(gpio_num), ESP_ERR_INVALID_ARG, TAG, "Invalid GPIO %d", gpio_num);

    rmt_tx_channel_config_t rmt_tx_channel_config = {
        .gpio_num = gpio_num,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = WS2812B_RESOLUTION,
        .mem_block_symbols = 64,
        .trans_queue_depth = 8,
        .flags.with_dma = 0,
    };

    // Attempt to create channel, auto-log error if fails
    ESP_RETURN_ON_ERROR(rmt_new_tx_channel(&rmt_tx_channel_config, channel), TAG, "RMT create failed on GPIO %d", gpio_num);

    return ESP_OK;
}

esp_err_t ws2812b_init(ws2812b_dev_t* ws2812b, gpio_num_t gpio_num, uint16_t pixel_num) {
    esp_err_t ret = ESP_OK;

    // 1. Validation
    ESP_GOTO_ON_FALSE(ws2812b, ESP_ERR_INVALID_ARG, err, TAG, "dev is NULL");
    ESP_GOTO_ON_FALSE(
        pixel_num > 0 && pixel_num <= WS2812B_MAX_PIXEL_NUM, ESP_ERR_INVALID_ARG, err, TAG, "pixel_num out of range (max=%d)", WS2812B_MAX_PIXEL_NUM);

    // 2. Clear device (important!)
    memset(ws2812b, 0, sizeof(ws2812b_dev_t));

    ws2812b->gpio_num = gpio_num;
    ws2812b->pixel_num = pixel_num;

    // 3. RMT Encoder Setup
    ESP_GOTO_ON_ERROR(rmt_new_encoder(&ws2812b->rmt_encoder), err, TAG, "Encoder creation failed");

    // 4. RMT Channel Setup
    ESP_GOTO_ON_ERROR(ws2812b_init_channel(gpio_num, pixel_num, &ws2812b->rmt_channel), err, TAG, "Channel init failed");

    // 5. Enable RMT
    ESP_GOTO_ON_ERROR(rmt_enable(ws2812b->rmt_channel), err, TAG, "RMT enable failed");

    // 6. Clear LEDs (buffer is already zeroed)
    ESP_GOTO_ON_ERROR(
        rmt_transmit(ws2812b->rmt_channel, ws2812b->rmt_encoder, ws2812b->buffer, pixel_num * 3, &rmt_tx_config), err, TAG, "Failed to clear LEDs");

    rmt_tx_wait_all_done(ws2812b->rmt_channel, RMT_TIMEOUT_MS);

    ESP_LOGI(TAG, "WS2812B initialized (GPIO=%d, pixels=%d)", gpio_num, pixel_num);

    return ESP_OK;

err:
    if(ws2812b->rmt_channel) {
        rmt_del_channel(ws2812b->rmt_channel);
        ws2812b->rmt_channel = NULL;
    }
    if(ws2812b->rmt_encoder) {
        rmt_del_encoder(ws2812b->rmt_encoder);
        ws2812b->rmt_encoder = NULL;
    }
    return ret;
}

esp_err_t ws2812b_write(ws2812b_dev_t* ws2812b, uint8_t* _buffer) {
    // 1. Validate Handle
    ESP_RETURN_ON_FALSE(ws2812b, ESP_ERR_INVALID_ARG, TAG, "Handle is NULL");

    // 2. Validate Source Buffer
    ESP_RETURN_ON_FALSE(_buffer, ESP_ERR_INVALID_ARG, TAG, "Source buffer is NULL");

    // 3. Validate Internal Buffer (Defensive Programming)
    ESP_RETURN_ON_FALSE(ws2812b->buffer, ESP_ERR_INVALID_STATE, TAG, "Internal buffer is not allocated");

    // 4. Perform Fast Copy
    memcpy(ws2812b->buffer, _buffer, ws2812b->pixel_num * 3 * sizeof(uint8_t));

    return ESP_OK;
}

esp_err_t ws2812b_wait_done(ws2812b_dev_t* ws2812b) {
    // 1. Safety Check
    ESP_RETURN_ON_FALSE(ws2812b, ESP_ERR_INVALID_ARG, TAG, "Handle is NULL");

    // 2. State Check
    ESP_RETURN_ON_FALSE(ws2812b->rmt_channel, ESP_ERR_INVALID_STATE, TAG, "RMT channel not initialized");

    // 3. Wait for Done
    return rmt_tx_wait_all_done(ws2812b->rmt_channel, RMT_TIMEOUT_MS);
}

esp_err_t ws2812b_show(ws2812b_dev_t* ws2812b) {
    // 1. Basic Pointer Validation
    ESP_RETURN_ON_FALSE(ws2812b, ESP_ERR_INVALID_ARG, TAG, "Handle is NULL");

    // 2. State Validation
    ESP_RETURN_ON_FALSE(ws2812b->rmt_channel && ws2812b->rmt_encoder, ESP_ERR_INVALID_STATE, TAG, "RMT not initialized");

    // 3. Transmit
    size_t payload_size = ws2812b->pixel_num * 3;

    ESP_RETURN_ON_ERROR(
        rmt_transmit(ws2812b->rmt_channel, ws2812b->rmt_encoder, ws2812b->buffer, payload_size, &rmt_tx_config), TAG, "Failed to transmit");

    return ESP_OK;
}

esp_err_t ws2812b_del(ws2812b_dev_t* ws2812b) {
    if(ws2812b == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // 1. Best-effort: turn off LEDs
    if(ws2812b->rmt_channel) {
        memset(ws2812b->buffer, 0, ws2812b->pixel_num * 3);

        if(rmt_transmit(ws2812b->rmt_channel, ws2812b->rmt_encoder, ws2812b->buffer, ws2812b->pixel_num * 3, &rmt_tx_config) == ESP_OK) {
            rmt_tx_wait_all_done(ws2812b->rmt_channel, RMT_TIMEOUT_MS);
        }
    }

    // 2. Teardown RMT channel
    if(ws2812b->rmt_channel) {
        rmt_disable(ws2812b->rmt_channel);
        rmt_del_channel(ws2812b->rmt_channel);
        ws2812b->rmt_channel = NULL;
    }

    // 3. Teardown encoder
    if(ws2812b->rmt_encoder) {
        rmt_del_encoder(ws2812b->rmt_encoder);
        ws2812b->rmt_encoder = NULL;
    }

    ESP_LOGI(TAG, "WS2812B (GPIO %d, pixels=%d) de-initialized", ws2812b->gpio_num, ws2812b->pixel_num);

    return ESP_OK;
}

esp_err_t ws2812b_set_pixel(ws2812b_dev_t* ws2812b, int pixel_idx, uint8_t red, uint8_t green, uint8_t blue) {
    // 1. Check if handle exists
    ESP_RETURN_ON_FALSE(ws2812b, ESP_ERR_INVALID_ARG, TAG, "Handle is NULL");

    // 2. Check for Buffer Overflow (CRITICAL)
    if(pixel_idx < 0 || pixel_idx >= ws2812b->pixel_num) {
        ESP_LOGE(TAG, "Pixel index %d out of bounds (Max: %d)", pixel_idx, ws2812b->pixel_num);
        return ESP_ERR_INVALID_ARG;
    }

    // 3. Set Color (GRB Format for WS2812B)
    uint32_t offset = pixel_idx * 3;
    ws2812b->buffer[offset + 0] = green;
    ws2812b->buffer[offset + 1] = red;
    ws2812b->buffer[offset + 2] = blue;

    return ESP_OK;
}

esp_err_t ws2812b_fill(ws2812b_dev_t* ws2812b, uint8_t red, uint8_t green, uint8_t blue) {
    // 1. Validation
    ESP_RETURN_ON_FALSE(ws2812b, ESP_ERR_INVALID_ARG, TAG, "Handle is NULL");

    // Defensive check: Ensure buffer was allocated
    ESP_RETURN_ON_FALSE(ws2812b->buffer, ESP_ERR_INVALID_STATE, TAG, "Buffer is NULL");

    // 2. Optimization check
    // If all colors are 0 (turning off), memset is significantly faster than a loop
    if(red == 0 && green == 0 && blue == 0) {
        memset(ws2812b->buffer, 0, ws2812b->pixel_num * 3);
        return ESP_OK;
    }

    // 3. Fill Buffer
    // Loop unrolling or pointer arithmetic could optimize this, but compiler usually handles it well.
    uint8_t* ptr = ws2812b->buffer;
    for(int i = 0; i < ws2812b->pixel_num; i++) {
        *ptr++ = green;  // G
        *ptr++ = red;    // R
        *ptr++ = blue;   // B
    }

    return ESP_OK;
}

esp_err_t ws2812b_print_buffer(ws2812b_dev_t* ws2812b) {
    // 1. Validation
    ESP_RETURN_ON_FALSE(ws2812b, ESP_ERR_INVALID_ARG, TAG, "Handle is NULL");
    ESP_RETURN_ON_FALSE(ws2812b->buffer, ESP_ERR_INVALID_STATE, TAG, "Buffer is NULL");

    // 2. Log Header
    ESP_LOGI(TAG, "Dumping Buffer (%d pixels, GRB format):", ws2812b->pixel_num);

    // 3. Hex Dump
    // ESP-IDF built-in function.
    // It prints the memory address offset and data in a readable 16-byte-per-line format.
    // LOG_LEVEL_INFO ensures it only prints if the log level is appropriate.
    ESP_LOG_BUFFER_HEXDUMP(TAG, ws2812b->buffer, ws2812b->pixel_num * 3, ESP_LOG_INFO);

    return ESP_OK;
}

esp_err_t ws2812b_get_pixel(ws2812b_dev_t* ws2812b, int pixel_idx, uint8_t* red, uint8_t* green, uint8_t* blue) {
    if(!ws2812b || pixel_idx < 0 || pixel_idx >= ws2812b->pixel_num) {
        return ESP_ERR_INVALID_ARG;
    }
    // GRB mapping
    *green = ws2812b->buffer[pixel_idx * 3 + 0];
    *red = ws2812b->buffer[pixel_idx * 3 + 1];
    *blue = ws2812b->buffer[pixel_idx * 3 + 2];
    return ESP_OK;
}
