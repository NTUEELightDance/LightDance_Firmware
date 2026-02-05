#include "ld_ws2812b_tx.h"

#include <stdlib.h>
#include <string.h>

#include "driver/rmt_tx.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "ld_ws2812b_tx";

struct ld_ws2812b_tx_t {
    ld_ws2812b_tx_config_t cfg;
    rmt_channel_handle_t tx_chan;
    rmt_encoder_handle_t encoder;
    bool enabled;
};

esp_err_t ld_ws2812b_tx_init(const ld_ws2812b_tx_config_t* cfg, ld_ws2812b_tx_handle_t* out) {
    esp_err_t ret = ESP_OK;

    ESP_RETURN_ON_FALSE(cfg, ESP_ERR_INVALID_ARG, TAG, "cfg is NULL");
    ESP_RETURN_ON_FALSE(out, ESP_ERR_INVALID_ARG, TAG, "out is NULL");
    ESP_RETURN_ON_FALSE(cfg->gpio_num >= 0, ESP_ERR_INVALID_ARG, TAG, "invalid gpio_num");
    ESP_RETURN_ON_FALSE(cfg->resolution_hz > 0, ESP_ERR_INVALID_ARG, TAG, "resolution_hz must be > 0");
    ESP_RETURN_ON_FALSE(cfg->mem_block_symbols > 0, ESP_ERR_INVALID_ARG, TAG, "mem_block_symbols must be > 0");
    ESP_RETURN_ON_FALSE(cfg->trans_queue_depth > 0, ESP_ERR_INVALID_ARG, TAG, "trans_queue_depth must be > 0");

    *out = NULL;

    ld_ws2812b_tx_t* h = (ld_ws2812b_tx_t*)calloc(1, sizeof(ld_ws2812b_tx_t));
    ESP_RETURN_ON_FALSE(h, ESP_ERR_NO_MEM, TAG, "no mem for handle");

    h->cfg = *cfg;
    h->tx_chan = NULL;
    h->encoder = NULL;
    h->enabled = false;

    // 1) Create TX channel
    rmt_tx_channel_config_t tx_cfg = {
        .gpio_num = cfg->gpio_num,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = cfg->resolution_hz,
        .mem_block_symbols = cfg->mem_block_symbols,
        .trans_queue_depth = cfg->trans_queue_depth,
        .flags =
            {
                .invert_out = 0,
                .with_dma = 0,
                .io_loop_back = 0,
            },
    };

    ret = rmt_new_tx_channel(&tx_cfg, &h->tx_chan);
    if(ret != ESP_OK) {
        ESP_LOGE(TAG, "rmt_new_tx_channel failed: %s", esp_err_to_name(ret));
        goto fail;
    }

    // 2) Create WS2812B encoder
    ret = ld_rmt_new_ws2812b_encoder(&cfg->enc, &h->encoder);
    if(ret != ESP_OK) {
        ESP_LOGE(TAG, "ld_rmt_new_ws2812b_encoder failed: %s", esp_err_to_name(ret));
        goto fail;
    }

    // 3) Enable channel (ready for transmit)
    ret = rmt_enable(h->tx_chan);
    if(ret != ESP_OK) {
        ESP_LOGE(TAG, "rmt_enable failed: %s", esp_err_to_name(ret));
        goto fail;
    }
    h->enabled = true;

    *out = h;
    return ESP_OK;

fail:
    if(h) {
        if(h->tx_chan && h->enabled) {
            (void)rmt_disable(h->tx_chan);
            h->enabled = false;
        }
        if(h->encoder) {
            (void)rmt_del_encoder(h->encoder);
            h->encoder = NULL;
        }
        if(h->tx_chan) {
            (void)rmt_del_channel(h->tx_chan);
            h->tx_chan = NULL;
        }
        free(h);
    }
    return ret;
}

esp_err_t ld_ws2812b_tx_deinit(ld_ws2812b_tx_handle_t h) {
    ESP_RETURN_ON_FALSE(h, ESP_ERR_INVALID_ARG, TAG, "h is NULL");

    // disable channel -> del encoder -> del channel -> free
    if(h->tx_chan && h->enabled) {
        (void)rmt_disable(h->tx_chan);
        h->enabled = false;
    }

    if(h->encoder) {
        (void)rmt_del_encoder(h->encoder);
        h->encoder = NULL;
    }

    if(h->tx_chan) {
        (void)rmt_del_channel(h->tx_chan);
        h->tx_chan = NULL;
    }

    free(h);
    return ESP_OK;
}

esp_err_t ld_ws2812b_tx_transmit_bytes(ld_ws2812b_tx_handle_t h, const uint8_t* bytes, size_t len) {
    ESP_RETURN_ON_FALSE(h, ESP_ERR_INVALID_ARG, TAG, "h is NULL");
    ESP_RETURN_ON_FALSE(bytes, ESP_ERR_INVALID_ARG, TAG, "bytes is NULL");
    ESP_RETURN_ON_FALSE(len > 0, ESP_ERR_INVALID_ARG, TAG, "len must be > 0");
    ESP_RETURN_ON_FALSE(h->tx_chan, ESP_ERR_INVALID_STATE, TAG, "tx channel not created");
    ESP_RETURN_ON_FALSE(h->encoder, ESP_ERR_INVALID_STATE, TAG, "encoder not created");

    if(!h->enabled) {
        esp_err_t ret = rmt_enable(h->tx_chan);
        ESP_RETURN_ON_ERROR(ret, TAG, "rmt_enable failed");
        h->enabled = true;
    }

    // transmit config
    // loop_count=0 => send once
    // eot_level=0 => line low after done (WS2812 idle is typically low)
    rmt_transmit_config_t tx_cfg = {
        .loop_count = 0,
        .flags =
            {
                .eot_level = 0,
            },
    };

    esp_err_t ret = rmt_transmit(h->tx_chan, h->encoder, bytes, len, &tx_cfg);
    ESP_RETURN_ON_ERROR(ret, TAG, "rmt_transmit failed");
    return ESP_OK;
}

esp_err_t ld_ws2812b_tx_wait_done(ld_ws2812b_tx_handle_t h, uint32_t timeout_ms) {
    ESP_RETURN_ON_FALSE(h, ESP_ERR_INVALID_ARG, TAG, "h is NULL");
    ESP_RETURN_ON_FALSE(h->tx_chan, ESP_ERR_INVALID_STATE, TAG, "tx channel not created");

    TickType_t ticks = (timeout_ms == UINT32_MAX) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);

    esp_err_t ret = rmt_tx_wait_all_done(h->tx_chan, ticks);
    if(ret == ESP_ERR_TIMEOUT) {
        return ESP_ERR_TIMEOUT;
    }
    ESP_RETURN_ON_ERROR(ret, TAG, "rmt_tx_wait_all_done failed");
    return ESP_OK;
}

bool ld_ws2812b_tx_busy(ld_ws2812b_tx_handle_t h) {
    if(!h || !h->tx_chan) {
        return false;
    }

    // 0 ticks => non-blocking probe
    esp_err_t ret = rmt_tx_wait_all_done(h->tx_chan, 0);

    if(ret == ESP_OK) {
        return false;  // all done
    }
    if(ret == ESP_ERR_TIMEOUT) {
        return true;  // still busy / pending
    }

    // Any other error: be conservative
    return true;
}

gpio_num_t ld_ws2812b_tx_gpio(ld_ws2812b_tx_handle_t h) {
    if(!h) {
        return GPIO_NUM_NC;
    }
    return h->cfg.gpio_num;
}