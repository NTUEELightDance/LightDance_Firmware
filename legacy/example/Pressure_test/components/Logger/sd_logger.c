#include "sd_logger.h"
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_vfs_fat.h"

static const char* TAG = "sd_logger";
static FILE* log_file = NULL;

static bool is_logging = false;
static vprintf_like_t default_vprintf = NULL;

//vprintf（直接寫入檔案）
static int sd_log_vprintf(const char *fmt, va_list l) {
    
    int ret_len = 0;
    if (is_logging && log_file) {
        va_list l_file;
        va_copy(l_file, l);
        int len = vfprintf(log_file, fmt, l_file);
        va_end(l_file);
        
        static int write_count = 0;
        if(++write_count >= 10 || strchr(fmt, '\n')) {
            fflush(log_file);
            write_count = 0;
        }
        ret_len = len;
    }
    return ret_len;
}

esp_err_t sd_logger_init(const char* log_path) {
    if(!log_path)
        return ESP_ERR_INVALID_ARG;
    if(is_logging)
        return ESP_ERR_INVALID_STATE;

    const char* vfs_path = log_path;
    log_file = fopen(vfs_path, "w+");

    if(log_file == NULL) {
        ESP_LOGE(TAG, "Failed to open log file: %s (errno: %d)", vfs_path, errno);
        return ESP_FAIL;
    }
    is_logging = true;

    default_vprintf = esp_log_set_vprintf(sd_log_vprintf);

    fprintf(log_file, "\n========== Logger Session Start ==========\n");
    fflush(log_file);

    ESP_LOGI(TAG, "SD Logger started at: %s", vfs_path);
    return ESP_OK;
}

void sd_logger_deinit(void) {
    if(!is_logging)
        return;

    fprintf(log_file, "========== Logger Session End ==========\n\n");
    is_logging = false;

    if(default_vprintf) {
        esp_log_set_vprintf(default_vprintf);
        default_vprintf = NULL;
    }

    if(log_file) {
        fflush(log_file);
        fclose(log_file);
        log_file = NULL;
    }

    ESP_LOGI(TAG, "SD Logger closed");
}

// direct into Log
int sd_log_printf(const char* format, ...) {
    if(!is_logging || !log_file) {
        return -1;
    }

    va_list args;
    va_start(args, format);
    int len = vfprintf(log_file, format, args);
    va_end(args);
    
    static int write_count = 0;
    if(++write_count >= 10 || strchr(format, '\n')) {
        fflush(log_file);
        write_count = 0;
    }
    
    return len;
}