#pragma once

#include <stdint.h>

#include "esp_bt.h"

void bt_hci_controller_init(esp_vhci_host_callback_t* callback);
void bt_hci_unregister_callback(void);
void bt_hci_controller_deinit(void);

void bt_hci_start_scan(void);
void bt_hci_stop_scan(void);
void bt_hci_set_scan_enabled(uint8_t enable);

void bt_hci_set_ack_adv_params(void);
void bt_hci_set_adv_data(uint8_t data_len, uint8_t* data);
void bt_hci_set_adv_enabled(uint8_t enable);
