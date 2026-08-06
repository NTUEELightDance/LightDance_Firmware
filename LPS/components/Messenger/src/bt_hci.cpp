#include "bt_hci.hpp"

#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// --- HCI Command Definitions ---
#define HCI_H4_CMD_PREAMBLE_SIZE (4)
#define HCI_GRP_HOST_CONT_BASEBAND_CMDS (0x03 << 10)
#define HCI_GRP_BLE_CMDS (0x08 << 10)
#define HCI_RESET (0x0003 | HCI_GRP_HOST_CONT_BASEBAND_CMDS)
#define HCI_SET_EVT_MASK (0x0001 | HCI_GRP_HOST_CONT_BASEBAND_CMDS)
#define HCI_BLE_WRITE_SCAN_PARAM (0x000B | HCI_GRP_BLE_CMDS)
#define HCI_BLE_WRITE_SCAN_ENABLE (0x000C | HCI_GRP_BLE_CMDS)
#define HCIC_PARAM_SIZE_SET_EVENT_MASK (8)
#define HCIC_PARAM_SIZE_BLE_WRITE_SCAN_PARAM (7)
#define HCIC_PARAM_SIZE_BLE_WRITE_SCAN_ENABLE (2)

// Helper macros to write data into HCI buffer
#define UINT16_TO_STREAM(p, u16)        \
    {                                   \
        *(p)++ = (uint8_t)(u16);        \
        *(p)++ = (uint8_t)((u16) >> 8); \
    }
#define UINT8_TO_STREAM(p, u8) \
    { *(p)++ = (uint8_t)(u8); }
#define ARRAY_TO_STREAM(p, a, len)     \
    {                                  \
        int ijk;                       \
        for(ijk = 0; ijk < len; ijk++) \
            *(p)++ = (uint8_t)a[ijk];  \
    }

enum { H4_TYPE_COMMAND = 1, H4_TYPE_ACL = 2, H4_TYPE_SCO = 3, H4_TYPE_EVENT = 4 };

static uint8_t hci_cmd_buf[128];

static uint16_t make_cmd_reset(uint8_t* buf) {
    UINT8_TO_STREAM(buf, H4_TYPE_COMMAND);
    UINT16_TO_STREAM(buf, HCI_RESET);
    UINT8_TO_STREAM(buf, 0);
    return HCI_H4_CMD_PREAMBLE_SIZE;
}

static uint16_t make_cmd_set_evt_mask(uint8_t* buf, uint8_t* evt_mask) {
    UINT8_TO_STREAM(buf, H4_TYPE_COMMAND);
    UINT16_TO_STREAM(buf, HCI_SET_EVT_MASK);
    UINT8_TO_STREAM(buf, HCIC_PARAM_SIZE_SET_EVENT_MASK);
    ARRAY_TO_STREAM(buf, evt_mask, HCIC_PARAM_SIZE_SET_EVENT_MASK);
    return HCI_H4_CMD_PREAMBLE_SIZE + HCIC_PARAM_SIZE_SET_EVENT_MASK;
}

static uint16_t make_cmd_ble_set_scan_params(uint8_t* buf,
                                             uint8_t scan_type,
                                             uint16_t scan_interval,
                                             uint16_t scan_window,
                                             uint8_t own_addr_type,
                                             uint8_t filter_policy) {
    UINT8_TO_STREAM(buf, H4_TYPE_COMMAND);
    UINT16_TO_STREAM(buf, HCI_BLE_WRITE_SCAN_PARAM);
    UINT8_TO_STREAM(buf, HCIC_PARAM_SIZE_BLE_WRITE_SCAN_PARAM);
    UINT8_TO_STREAM(buf, scan_type);
    UINT16_TO_STREAM(buf, scan_interval);
    UINT16_TO_STREAM(buf, scan_window);
    UINT8_TO_STREAM(buf, own_addr_type);
    UINT8_TO_STREAM(buf, filter_policy);
    return HCI_H4_CMD_PREAMBLE_SIZE + HCIC_PARAM_SIZE_BLE_WRITE_SCAN_PARAM;
}

static uint16_t make_cmd_ble_set_scan_enable(uint8_t* buf, uint8_t scan_enable, uint8_t filter_duplicates) {
    UINT8_TO_STREAM(buf, H4_TYPE_COMMAND);
    UINT16_TO_STREAM(buf, HCI_BLE_WRITE_SCAN_ENABLE);
    UINT8_TO_STREAM(buf, HCIC_PARAM_SIZE_BLE_WRITE_SCAN_ENABLE);
    UINT8_TO_STREAM(buf, scan_enable);
    UINT8_TO_STREAM(buf, filter_duplicates);
    return HCI_H4_CMD_PREAMBLE_SIZE + HCIC_PARAM_SIZE_BLE_WRITE_SCAN_ENABLE;
}

static uint16_t make_cmd_ble_set_adv_data(uint8_t* buf, uint8_t data_len, uint8_t* p_data) {
    UINT8_TO_STREAM(buf, H4_TYPE_COMMAND);
    UINT16_TO_STREAM(buf, 0x2008); // HCI_BLE_WRITE_ADV_DATA
    UINT8_TO_STREAM(buf, 32);      // HCI Param Length (Fixed 32)
    UINT8_TO_STREAM(buf, data_len); // Adv Data Length
    ARRAY_TO_STREAM(buf, p_data, data_len);
    uint8_t pad_len = 31 - data_len;
    for (int i = 0; i < pad_len; i++) {
        UINT8_TO_STREAM(buf, 0); // Padding zeros
    }
    return HCI_H4_CMD_PREAMBLE_SIZE + 1 + 31;
}

void bt_hci_controller_init(esp_vhci_host_callback_t* callback) {
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_bt_controller_init(&bt_cfg);
    esp_bt_controller_enable(ESP_BT_MODE_BLE);
    esp_vhci_host_register_callback(callback);
}

void bt_hci_unregister_callback(void) {
    esp_vhci_host_register_callback(NULL);
}

void bt_hci_controller_deinit(void) {
    esp_bt_controller_disable();
    esp_bt_controller_deinit();
    esp_bt_mem_release(ESP_BT_MODE_BTDM);
}

void bt_hci_start_scan(void) {
    uint16_t sz = make_cmd_reset(hci_cmd_buf);
    esp_vhci_host_send_packet(hci_cmd_buf, sz);
    vTaskDelay(pdMS_TO_TICKS(20));

    uint8_t mask[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20};
    sz = make_cmd_set_evt_mask(hci_cmd_buf, mask);
    esp_vhci_host_send_packet(hci_cmd_buf, sz);
    vTaskDelay(pdMS_TO_TICKS(20));

    sz = make_cmd_ble_set_scan_params(hci_cmd_buf, 0x00, 0x0F, 0x0F, 0x00, 0x00);
    esp_vhci_host_send_packet(hci_cmd_buf, sz);
    vTaskDelay(pdMS_TO_TICKS(20));

    sz = make_cmd_ble_set_scan_enable(hci_cmd_buf, 1, 0);
    esp_vhci_host_send_packet(hci_cmd_buf, sz);
}

void bt_hci_stop_scan(void) {
    uint16_t sz = make_cmd_ble_set_scan_enable(hci_cmd_buf, 0, 0);
    esp_vhci_host_send_packet(hci_cmd_buf, sz);
}

void bt_hci_set_scan_enabled(uint8_t enable) {
    uint8_t scan_buf[32];
    make_cmd_ble_set_scan_enable(scan_buf, enable, 0);
    esp_vhci_host_send_packet(scan_buf, 6);
}

void bt_hci_set_ack_adv_params(void) {
    uint8_t buf[128];
    uint8_t* p = buf;
    uint16_t interval = 32;

    UINT8_TO_STREAM(p, H4_TYPE_COMMAND);
    UINT16_TO_STREAM(p, 0x2006); // HCI_BLE_WRITE_ADV_PARAMS
    UINT8_TO_STREAM(p, 15);
    UINT16_TO_STREAM(p, interval); // Min
    UINT16_TO_STREAM(p, interval); // Max

    // Use ADV_IND (Type 0)
    UINT8_TO_STREAM(p, 0);

    UINT8_TO_STREAM(p, 0); // Own addr type
    UINT8_TO_STREAM(p, 0); // Peer addr type
    UINT8_TO_STREAM(p, 0); UINT8_TO_STREAM(p, 0); UINT8_TO_STREAM(p, 0); UINT8_TO_STREAM(p, 0); UINT8_TO_STREAM(p, 0); UINT8_TO_STREAM(p, 0);
    UINT8_TO_STREAM(p, 0x07); // Channel Map
    UINT8_TO_STREAM(p, 0); // Filter Policy

    esp_vhci_host_send_packet(buf, p - buf);
}

void bt_hci_set_adv_data(uint8_t data_len, uint8_t* data) {
    uint8_t hci_buf[128];
    uint16_t pkt_len = make_cmd_ble_set_adv_data(hci_buf, data_len, data);
    esp_vhci_host_send_packet(hci_buf, pkt_len);
}

void bt_hci_set_adv_enabled(uint8_t enable) {
    uint8_t buf[128];
    uint8_t* p = buf;
    UINT8_TO_STREAM(p, H4_TYPE_COMMAND);
    UINT16_TO_STREAM(p, 0x200A); // HCI_BLE_WRITE_ADV_ENABLE
    UINT8_TO_STREAM(p, 1);
    UINT8_TO_STREAM(p, enable);
    esp_vhci_host_send_packet(buf, p - buf);
}
