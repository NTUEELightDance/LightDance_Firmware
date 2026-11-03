#pragma once

#include <stdbool.h>

#include "bt_receiver.h"
#include "esp_bt.h"

esp_err_t bt_scheduler_init(const bt_receiver_config_t* config);
void bt_scheduler_start(void);
void bt_scheduler_request_stop(void);
void bt_scheduler_stop_timers(void);
void bt_scheduler_deinit(void);

bool bt_scheduler_is_running(void);
esp_vhci_host_callback_t* bt_scheduler_vhci_callback(void);
