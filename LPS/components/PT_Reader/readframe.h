#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "ld_frame.h"

#ifdef __cplusplus
extern "C" {
#endif

extern ch_info_t ch_info_snapshot;

esp_err_t frame_system_init(const char* control_path, const char* frame_path);
esp_err_t read_frame(table_frame_t* out);

/**
 * Reposition the stream so the next two read_frame() calls bracket time_ms.
 *
 * The first read_frame() after seek returns the frame at or before time_ms
 * when such a frame exists. The second read_frame() returns the following frame.
 */
esp_err_t read_frame_seek(uint64_t time_ms);

esp_err_t frame_reset(void);
esp_err_t frame_system_deinit(void);
bool is_eof_reached(void);

#ifdef __cplusplus
}
#endif
