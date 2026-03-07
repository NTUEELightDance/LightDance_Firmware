#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// Wi-Fi & TCP Server Config
#define TCP_WIFI_SSID      "makerspace-2.4G"
#define TCP_WIFI_PASS      "ntueemakerspace"
#define TCP_SERVER_IP      "192.168.0.14" // server IP
#define TCP_SERVER_PORT    3333            // server port

/**
 * @brief  process to update files via TCP:
 * * 
 * 1. bt_receiver_deinit()
 * 2. Connect Wi-Fi & TCP Server
 * 3. Send Player ID
 * 4. Recv & Write "control.dat" then "frame.dat"
 * 5. Disconnect Wi-Fi
 * 6. bt_receiver_init() & start()
 */
void tcp_client_start_update_task(void);

#ifdef __cplusplus
}
#endif