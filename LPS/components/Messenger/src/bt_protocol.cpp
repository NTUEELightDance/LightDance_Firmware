#include "bt_protocol.hpp"

#include <string.h>

#include "esp_timer.h"

bool IRAM_ATTR bt_protocol_parse_hci_event(uint8_t* data, uint16_t len, int my_player_id, ble_rx_packet_t* packet) {
    int64_t now_us = esp_timer_get_time();
    if(data[0] != 0x04 || data[1] != 0x3E || data[3] != 0x02)
        return false;

    uint8_t num_reports = data[4];
    uint8_t* payload = &data[5];

    for(int i = 0; i < num_reports; i++) {
        uint8_t* mac = &payload[2];
        uint8_t data_len = payload[8];
        uint8_t* adv_data = &payload[9];
        int8_t rssi = payload[9 + data_len];

        uint8_t offset = 0;
        while(offset < data_len) {
            uint8_t ad_len = adv_data[offset++];
            if(ad_len == 0)
                break;
            uint8_t ad_type = adv_data[offset++];
            if(ad_type == 0xFF && ad_len == 22) {
                if(adv_data[offset] == 0xFF && adv_data[offset + 1] == 0xFF && adv_data[offset + 2] == 0x4C && adv_data[offset + 3] == 0x44) {
                    uint64_t rcv_mask = 0;
                    for(int k = 0; k < 8; k++)
                        rcv_mask |= ((uint64_t)adv_data[offset + 5 + k] << (k * 8));

                    bool is_target = false;
                    if(rcv_mask == 0xFFFFFFFFFFFFFFFFULL) {
                        is_target = true;
                    } else {
                        if((rcv_mask >> my_player_id) & 1ULL) {
                            is_target = true;
                        }
                    }
                    if(is_target) {
                        uint8_t rcv_cmd_id = (adv_data[offset + 4] >> 4) & 0x0F;
                        uint8_t rcv_cmd = adv_data[offset + 4] & 0x0F;
                        uint32_t rcv_delay_ms = (adv_data[offset + 13] << 24) | (adv_data[offset + 14] << 16) | (adv_data[offset + 15] << 8) | (adv_data[offset + 16]);
                        uint32_t rcv_prep_ms = 0;
                        uint32_t rcv_target_ms = 0;
                        uint8_t rcv_data[3] = {0, 0, 0};

                        int spec_idx = offset + 17;
                        if(rcv_cmd == 0x01) {
                            rcv_prep_ms = (adv_data[spec_idx] << 24) | (adv_data[spec_idx + 1] << 16) | (adv_data[spec_idx + 2] << 8) | adv_data[spec_idx + 3];
                        } else if(rcv_cmd == 0x05) {
                            rcv_data[0] = adv_data[spec_idx];
                            rcv_data[1] = adv_data[spec_idx + 1];
                            rcv_data[2] = adv_data[spec_idx + 2];
                        } else if(rcv_cmd == 0x06) {
                            rcv_data[0] = adv_data[spec_idx];
                        } else if(rcv_cmd == 0x0A) {
                            rcv_target_ms = (adv_data[spec_idx] << 24) | (adv_data[spec_idx + 1] << 16) | (adv_data[spec_idx + 2] << 8) | adv_data[spec_idx + 3];
                        }
                        packet->cmd_id = rcv_cmd_id;
                        packet->cmd_type = rcv_cmd;
                        packet->target_mask = rcv_mask;
                        packet->delay_val = rcv_delay_ms * 1000ULL;
                        packet->prep_time = rcv_prep_ms * 1000ULL;
                        packet->target_time = rcv_target_ms * 1000ULL;
                        packet->data[0] = rcv_data[0];
                        packet->data[1] = rcv_data[1];
                        packet->data[2] = rcv_data[2];
                        packet->rssi = rssi;
                        packet->rx_time_us = now_us;
                        memcpy(packet->mac, mac, 6);
                        return true;
                    }
                }
            }
            offset += (ad_len - 1);
        }
        payload += (10 + data_len + 1);
    }

    return false;
}

uint8_t bt_protocol_build_ack_adv_data(uint8_t my_id, uint8_t cmd_id, uint8_t cmd_type, uint32_t delay_val, uint8_t state, uint8_t* raw_data) {
    uint8_t idx = 0;

    raw_data[idx++] = 2;
    raw_data[idx++] = 0x01;
    raw_data[idx++] = 0x06;

    // Payload Length = 14
    raw_data[idx++] = 14;
    raw_data[idx++] = 0xFF;
    raw_data[idx++] = 0xFF;
    raw_data[idx++] = 0xFF;

    raw_data[idx++] = 0x4C;
    raw_data[idx++] = 0x44;
    raw_data[idx++] = 0x07;

    raw_data[idx++] = my_id;
    raw_data[idx++] = cmd_id;
    raw_data[idx++] = cmd_type;

    uint32_t delay_ms = delay_val / 1000;
    raw_data[idx++] = (delay_ms >> 24) & 0xFF;
    raw_data[idx++] = (delay_ms >> 16) & 0xFF;
    raw_data[idx++] = (delay_ms >> 8) & 0xFF;
    raw_data[idx++] = (delay_ms) & 0xFF;

    raw_data[idx++] = state;
    return idx;
}
