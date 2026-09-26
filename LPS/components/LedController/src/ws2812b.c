#include "ws2812b.h"

#include "string.h"

#include "esp_check.h"
#include "esp_log.h"

static const char* TAG = "WS2812B";

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
    ESP_RETURN_ON_FALSE(channel, ESP_ERR_INVALID_ARG, TAG, "RMT channel initialization rejected: output handle pointer is NULL");
    ESP_RETURN_ON_FALSE(GPIO_IS_VALID_OUTPUT_GPIO(gpio_num), ESP_ERR_INVALID_ARG, TAG, "RMT channel initialization rejected: invalid output GPIO=%d", gpio_num);

    rmt_tx_channel_config_t rmt_tx_channel_config = {
        .gpio_num = gpio_num,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = LD_BOARD_WS2812B_RMT_RESOLUTION_HZ,
        .intr_priority = 0,
        .mem_block_symbols = LD_BOARD_RMT_MEM_BLOCK_SYMBOLS,
        .trans_queue_depth = LD_BOARD_RMT_TRANS_QUEUE_DEPTH,
        .flags.with_dma = 0,
    };

    // Attempt to create channel, auto-log error if fails
    ESP_RETURN_ON_ERROR(rmt_new_tx_channel(&rmt_tx_channel_config, channel), TAG, "RMT TX channel creation failed: gpio=%d", gpio_num);

    return ESP_OK;
}

esp_err_t ws2812b_init(ws2812b_dev_t* ws2812b, gpio_num_t gpio_num, uint16_t pixel_num) {
    esp_err_t ret = ESP_OK;

    // 1. Validation
    ESP_GOTO_ON_FALSE(ws2812b, ESP_ERR_INVALID_ARG, err, TAG, "device initialization rejected: device pointer is NULL");
    ESP_GOTO_ON_FALSE(pixel_num > 0 && pixel_num <= LD_BOARD_WS2812B_MAX_PIXEL_NUM,
                      ESP_ERR_INVALID_ARG,
                      err,
                      TAG,
                      "device initialization rejected: pixels=%u valid_range=1..%d",
                      pixel_num,
                      LD_BOARD_WS2812B_MAX_PIXEL_NUM);

    // 2. Clear device (important!)
    memset(ws2812b, 0, sizeof(ws2812b_dev_t));

    ws2812b->gpio_num = gpio_num;
    ws2812b->pixel_num = pixel_num;

    // 3. RMT Encoder Setup
    ESP_GOTO_ON_ERROR(rmt_new_encoder(&ws2812b->rmt_encoder), err, TAG, "RMT encoder creation failed: gpio=%d", gpio_num);

    // 4. RMT Channel Setup
    ESP_GOTO_ON_ERROR(ws2812b_init_channel(gpio_num, pixel_num, &ws2812b->rmt_channel), err, TAG, "RMT channel initialization failed: gpio=%d", gpio_num);
    ESP_GOTO_ON_ERROR(gpio_set_drive_capability(gpio_num, GPIO_DRIVE_CAP_0), err, TAG, "GPIO drive capability setup failed: gpio=%d", gpio_num);

    // 5. Enable RMT
    ESP_GOTO_ON_ERROR(rmt_enable(ws2812b->rmt_channel), err, TAG, "RMT channel enable failed: gpio=%d", gpio_num);

    // 6. Clear LEDs (buffer is already zeroed)
    ESP_GOTO_ON_ERROR(rmt_transmit(ws2812b->rmt_channel, ws2812b->rmt_encoder, ws2812b->buffer, pixel_num * 3, &rmt_tx_config),
                      err,
                      TAG,
                      "initial blackout transmission failed: gpio=%d pixels=%u",
                      gpio_num,
                      pixel_num);

    rmt_tx_wait_all_done(ws2812b->rmt_channel, LD_CFG_RMT_TIMEOUT_MS);

    ESP_LOGD(TAG, "device initialized: gpio=%d pixels=%u", gpio_num, pixel_num);

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

esp_err_t ws2812b_write_grb(ws2812b_dev_t* ws2812b, const grb8_t* colors, uint16_t count) {
    // 1. Validate Handle
    ESP_RETURN_ON_FALSE(ws2812b, ESP_ERR_INVALID_ARG, TAG, "buffer write rejected: device pointer is NULL");

    // 2. Validate Source Buffer
    ESP_RETURN_ON_FALSE(colors, ESP_ERR_INVALID_ARG, TAG, "buffer write rejected: source color buffer is NULL");
    ESP_RETURN_ON_FALSE(count <= ws2812b->pixel_num, ESP_ERR_INVALID_ARG, TAG, "buffer write rejected: requested_pixels=%u configured_pixels=%u", count, ws2812b->pixel_num);

    // 4. Perform Fast Copy
    memcpy(ws2812b->buffer, (const uint8_t*)colors, count * sizeof(grb8_t));

    return ESP_OK;
}

esp_err_t ws2812b_wait_done(ws2812b_dev_t* ws2812b) {
    // 1. Safety Check
    ESP_RETURN_ON_FALSE(ws2812b, ESP_ERR_INVALID_ARG, TAG, "transmission wait rejected: device pointer is NULL");

    // 2. State Check
    ESP_RETURN_ON_FALSE(ws2812b->rmt_channel, ESP_ERR_INVALID_STATE, TAG, "transmission wait rejected: RMT channel is not initialized gpio=%d", ws2812b->gpio_num);

    // 3. Wait for Done
    return rmt_tx_wait_all_done(ws2812b->rmt_channel, LD_CFG_RMT_TIMEOUT_MS);
}

esp_err_t ws2812b_show(ws2812b_dev_t* ws2812b) {
    // 1. Basic Pointer Validation
    ESP_RETURN_ON_FALSE(ws2812b, ESP_ERR_INVALID_ARG, TAG, "transmission rejected: device pointer is NULL");

    // 2. State Validation
    ESP_RETURN_ON_FALSE(ws2812b->rmt_channel && ws2812b->rmt_encoder, ESP_ERR_INVALID_STATE, TAG, "transmission rejected: RMT resources are not initialized gpio=%d", ws2812b->gpio_num);

    // 3. Transmit
    size_t payload_size = ws2812b->pixel_num * 3;

    ESP_RETURN_ON_ERROR(rmt_transmit(ws2812b->rmt_channel, ws2812b->rmt_encoder, ws2812b->buffer, payload_size, &rmt_tx_config),
                        TAG,
                        "RMT transmission failed: gpio=%d payload_size=%u bytes",
                        ws2812b->gpio_num,
                        (unsigned)payload_size);

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
            rmt_tx_wait_all_done(ws2812b->rmt_channel, LD_CFG_RMT_TIMEOUT_MS);
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

    ESP_LOGD(TAG, "device deinitialized: gpio=%d pixels=%u", ws2812b->gpio_num, ws2812b->pixel_num);

    return ESP_OK;
}

esp_err_t ws2812b_set_pixel(ws2812b_dev_t* ws2812b, int pixel_idx, grb8_t color) {
    // 1. Check if handle exists
    ESP_RETURN_ON_FALSE(ws2812b, ESP_ERR_INVALID_ARG, TAG, "pixel write rejected: device pointer is NULL");

    // 2. Check for Buffer Overflow (CRITICAL)
    if(pixel_idx < 0 || pixel_idx >= ws2812b->pixel_num) {
        ESP_LOGE(TAG, "pixel write rejected: index=%d valid_range=0..%d gpio=%d", pixel_idx, ws2812b->pixel_num - 1, ws2812b->gpio_num);
        return ESP_ERR_INVALID_ARG;
    }

    // 3. Set Color (GRB Format for WS2812B)
    uint32_t offset = pixel_idx * 3;
    ws2812b->buffer[offset + 0] = color.g;
    ws2812b->buffer[offset + 1] = color.r;
    ws2812b->buffer[offset + 2] = color.b;

    return ESP_OK;
}

esp_err_t ws2812b_fill(ws2812b_dev_t* ws2812b, grb8_t color) {
    // 1. Validation
    ESP_RETURN_ON_FALSE(ws2812b, ESP_ERR_INVALID_ARG, TAG, "fill rejected: device pointer is NULL");

    // 2. Optimization check
    // If all colors are 0 (turning off), memset is significantly faster than a loop
    if(color.r == 0 && color.g == 0 && color.b == 0) {
        memset(ws2812b->buffer, 0, ws2812b->pixel_num * 3);
        return ESP_OK;
    }

    // 3. Fill Buffer
    // Loop unrolling or pointer arithmetic could optimize this, but compiler usually handles it well.
    uint8_t* ptr = ws2812b->buffer;
    for(int i = 0; i < ws2812b->pixel_num; i++) {
        *ptr++ = color.g;  // G
        *ptr++ = color.r;  // R
        *ptr++ = color.b;  // B
    }

    return ESP_OK;
}

esp_err_t ws2812b_print_buffer(ws2812b_dev_t* ws2812b) {
    // 1. Validation
    ESP_RETURN_ON_FALSE(ws2812b, ESP_ERR_INVALID_ARG, TAG, "buffer dump rejected: device pointer is NULL");

    // 2. Log Header
    ESP_LOGV(TAG, "buffer dump: gpio=%d pixels=%u format=GRB", ws2812b->gpio_num, ws2812b->pixel_num);

    // 3. Hex Dump
    // ESP-IDF built-in function.
    // It prints the memory address offset and data in a readable 16-byte-per-line format.
    // LOG_LEVEL_INFO ensures it only prints if the log level is appropriate.
    ESP_LOG_BUFFER_HEXDUMP(TAG, ws2812b->buffer, ws2812b->pixel_num * 3, ESP_LOG_VERBOSE);

    return ESP_OK;
}

esp_err_t ws2812b_get_pixel(ws2812b_dev_t* ws2812b, int pixel_idx, grb8_t* out_color) {
    if(!ws2812b || !out_color || pixel_idx < 0 || pixel_idx >= ws2812b->pixel_num) {
        return ESP_ERR_INVALID_ARG;
    }
    // GRB mapping
    out_color->g = ws2812b->buffer[pixel_idx * 3 + 0];
    out_color->r = ws2812b->buffer[pixel_idx * 3 + 1];
    out_color->b = ws2812b->buffer[pixel_idx * 3 + 2];
    return ESP_OK;
}
