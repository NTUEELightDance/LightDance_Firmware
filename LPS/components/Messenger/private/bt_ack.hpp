#pragma once

#include <stdint.h>

void bt_ack_trigger(uint8_t my_id,
                    uint8_t cmd_id,
                    uint8_t cmd_type,
                    uint32_t delay_val,
                    uint8_t state);
