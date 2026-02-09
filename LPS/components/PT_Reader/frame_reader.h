#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "frame.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Frame Reader API
 *
 * 依賴：
 *   - control.dat 已由 get_channel_info() 載入
 *   - global ch_info 已正確初始化
 *
 * frame.dat 格式：
 *   [uint16_t version]
 *   [frame0][frame1][frame2]...
 *
 * 每個 frame layout 由 ch_info 決定
 * ============================================================ */

/**
 * @brief  初始化 frame reader，並開啟 frame.dat
 *
 * @param  path   frame.dat 路徑（例如 "0:/frame.dat"）
 *
 * @return
 *   - ESP_OK                成功
 *   - ESP_ERR_INVALID_ARG   path 為 NULL
 *   - ESP_ERR_INVALID_STATE ch_info 尚未載入或為空
 *   - ESP_ERR_NOT_FOUND     檔案不存在
 *   - ESP_ERR_INVALID_SIZE  frame size 超過系統上限
 *   - ESP_FAIL              其他 I/O 錯誤
 */
esp_err_t frame_reader_init(const char *path);

/**
 * @brief  關閉 frame.dat 並釋放 reader 狀態
 *
 * 安全可重入（未 init 呼叫不會出錯）
 */
void frame_reader_deinit(void);

/**
 * @brief  回傳目前 frame 的 byte size
 *
 * @return frame size（bytes），若尚未 init 則為 0
 */
uint32_t frame_reader_frame_size(void);

/**
 * @brief  讀取下一個 frame
 *
 * @param[out] out  由 caller 提供的 frame buffer
 *
 * @return
 *   - ESP_OK                成功讀取一個 frame
 *   - ESP_ERR_INVALID_STATE 尚未 init
 *   - ESP_ERR_INVALID_ARG   out 為 NULL
 *   - ESP_ERR_NOT_FOUND     EOF 或無法再讀
 *   - ESP_ERR_INVALID_CRC   checksum mismatch（檔案指標已回復）
 */
esp_err_t frame_reader_read(table_frame_t *out);

esp_err_t frame_reader_reset(void);

#ifdef __cplusplus
}
#endif