#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "ws2812b_tx.h"

static const char* TAG = "ex_ws2812b_tx";

#define EX_WS2812B_GPIO GPIO_NUM_17
#define EX_WS2812B_PIXELS 100

static void example_ws2812b_tx(void) {
    ld_ws2812b_tx_config_t cfg = ld_ws2812b_tx_config_t_default();
    cfg.gpio_num = EX_WS2812B_GPIO;

    ld_ws2812b_tx_handle_t h = NULL;
    ESP_ERROR_CHECK(ld_ws2812b_tx_init(&cfg, &h));

    uint8_t data[EX_WS2812B_PIXELS * 3];
    memset(data, 50, sizeof(data));

    ESP_ERROR_CHECK(ld_ws2812b_tx_transmit_bytes(h, data, sizeof(data)));
    ESP_ERROR_CHECK(ld_ws2812b_tx_wait_done(h, LD_WS2812B_TX_DEFAULT_WAIT_DONE_TIMEOUT_MS));

    vTaskDelay(pdMS_TO_TICKS(1000));

    memset(data, 0, sizeof(data));
    ESP_ERROR_CHECK(ld_ws2812b_tx_transmit_bytes(h, data, sizeof(data)));
    ESP_ERROR_CHECK(ld_ws2812b_tx_wait_done(h, LD_WS2812B_TX_DEFAULT_WAIT_DONE_TIMEOUT_MS));

    ESP_ERROR_CHECK(ld_ws2812b_tx_deinit(h));
}
