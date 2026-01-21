#pragma once
#ifdef __cplusplus
extern "C" {
#endif
#include <stdint.h>
#include "esp_err.h"
#include "color.h"
#include "BoardConfig.h"
#include "player_protocal.h"
#include "channel_info.h"
#include "BoardConfig.h"
#include "frame_config.h"

ch_info_t get_channel_info(void);

esp_err_t frame_system_init(const char *control_path,
                            const char *frame_path);

esp_err_t read_frame(table_frame_t *playerbuffer);
esp_err_t read_frame_ts(table_frame_t *playerbuffer, uint64_t ts);

esp_err_t frame_reset(void);
void      frame_system_deinit(void);
// static esp_err_t mount_sdcard(void);
#ifdef __cplusplus
}
#endif