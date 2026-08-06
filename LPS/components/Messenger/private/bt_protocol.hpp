#pragma once

#include <stdint.h>

#include "bt_receiver.h"
#include "esp_attr.h"

bool IRAM_ATTR bt_protocol_parse_hci_event(uint8_t* data,
                                           uint16_t len,
                                           int my_player_id,
                                           ble_rx_packet_t* packet);

uint8_t bt_protocol_build_ack_adv_data(uint8_t my_id,
                                       uint8_t cmd_id,
                                       uint8_t cmd_type,
                                       uint32_t delay_val,
                                       uint8_t state,
                                       uint8_t* raw_data);
