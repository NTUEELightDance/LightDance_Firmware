#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "player.hpp"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*bt_receiver_callback_t)(void);

// --- Received Packet ---
typedef struct {
    uint8_t cmd_id;
    uint8_t cmd_type;
    uint64_t target_mask;
    uint32_t delay_val;
    uint32_t prep_time;
    uint8_t data[3];
    int8_t rssi;
    int64_t rx_time_us; 
} ble_rx_packet_t;

// --- Init Config ---
typedef struct {
    int feedback_gpio_num;      
    uint16_t manufacturer_id;   
    int my_player_id; 
    uint32_t sync_window_us;    
    uint32_t queue_size;        
} bt_receiver_config_t;

esp_err_t bt_receiver_init(const bt_receiver_config_t *config);
esp_err_t bt_receiver_start(void);
esp_err_t bt_receiver_stop(void);

#ifdef __cplusplus
}
#endif