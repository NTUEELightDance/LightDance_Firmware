#include "frame_reader.h"

#include <string.h>
#include "esp_log.h"
#include "ff.h"
#include "ld_board.h"  // global ch_info
#include "readframe.h"
#include "esp_err.h"

#include "frame_errors.h"

static const uint8_t EXPECTED_VERSION_MAJOR = 1;
static const uint8_t EXPECTED_VERSION_MINOR = 2;

#define FRAME_RAW_MAX_SIZE 8192
#define CHECKSUM_SIZE 4  // uint8 (reserved)
#define FRAME_DATA_OFFSET 2



static const char* TAG = "frame_reader";

static FIL fp;
static bool opened = false;
static uint32_t g_frame_size = 0;


static inline void checksum_add_u8(uint32_t* sum, uint8_t b) {
    *sum += (uint32_t)b;
}

/* ================= init / deinit ================= */

esp_err_t frame_reader_init(const char* path) {
    if(!path){
        ESP_LOGE(TAG, "path is NULL");
        return ESP_ERR_INVALID_ARG;
    }


    uint32_t of_cnt = 0;
    uint32_t led_cnt = 0;

    for(int i = 0; i < LD_BOARD_PCA9955B_CH_NUM; i++)
        if(ch_info_snapshot.i2c_leds[i])
            of_cnt++;

    for(int i = 0; i < LD_BOARD_WS2812B_NUM; i++)
        led_cnt += ch_info_snapshot.rmt_strips[i];

    if(of_cnt == 0 && led_cnt == 0) {
        ESP_LOGE(TAG, "ch_info empty (no OF, no LED)");
        return ESP_ERR_INVALID_STATE;
    }


    FRESULT fr = f_open(&fp, path, FA_READ);
    if(fr != FR_OK) {
        ESP_LOGE(TAG, "open %s failed (fr=%d)", path, fr);
        return ESP_ERR_NOT_FOUND;
    }


    uint8_t version_bytes[2];
    UINT br;
    fr = f_read(&fp, version_bytes, 2, &br);
    
    if(fr != FR_OK || br != 2) {
        ESP_LOGE(TAG, "Failed to read version header");
        f_close(&fp);
        return ESP_FAIL;
    }
    
    uint8_t major = version_bytes[0];
    uint8_t minor = version_bytes[1];
    
    if(major != EXPECTED_VERSION_MAJOR || minor != EXPECTED_VERSION_MINOR) {
        ESP_LOGE(TAG, "Version mismatch! Expected %d.%d, got %d.%d", 
                 EXPECTED_VERSION_MAJOR, EXPECTED_VERSION_MINOR, major, minor);
        f_close(&fp);
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "frame.dat version: %d.%d (OK)", major, minor);

    //framesize check

    g_frame_size = 4 +         
                   1 +             
                   (of_cnt * 3) +  
                   (led_cnt * 3) + 
                   CHECKSUM_SIZE;  

    if(g_frame_size > FRAME_RAW_MAX_SIZE) {
        ESP_LOGE(TAG, "frame_size %u exceeds max", (unsigned)g_frame_size);
        f_close(&fp);
        return ESP_ERR_INVALID_SIZE;
    }

    opened = true;

    ESP_LOGI(TAG, "frame_reader init: frame_size=%u (OF=%u LED=%u)", (unsigned)g_frame_size, (unsigned)of_cnt, (unsigned)led_cnt);

    return ESP_OK;
}

void frame_reader_deinit(void) {
    if(!opened)
        return;

    f_close(&fp);
    opened = false;
}

esp_err_t frame_reader_reset(void) {
    if(!opened){
        ESP_LOGE(TAG, "frame_reader not opened");
        return ESP_ERR_INVALID_STATE;
    }
    FRESULT fr = f_lseek(&fp, FRAME_DATA_OFFSET); // skip version header

    if(fr != FR_OK){
        ESP_LOGE(TAG, "f_lseek reset failed (fr=%d)", fr);
        return ESP_FAIL;
    }

    return ESP_OK;
}

void set_status(frame_reader_status* st, esp_err_t err, FRESULT fr, UINT br){
    if (!st) return;
    st -> err = err;
    st -> fr = fr;
    st -> br = br;
}
/* ================= read one frame ================= */

esp_err_t frame_reader_read(table_frame_t* out, frame_reader_status* st) {
    // if (memcmp(&ch_info, &ch_info_snapshot, sizeof(ch_info)) != 0) {
    // ESP_LOGE(TAG, "ch_info changed after init");
    // return ESP_FAIL;
    // }
    if(!opened){
        set_status(st, ESP_ERR_INVALID_STATE, FR_INVALID_OBJECT, 0);
        ESP_LOGE(TAG, "frame_reader not opened");
        return ESP_ERR_INVALID_STATE;
    }
    if(!out){
        set_status(st, ESP_ERR_INVALID_ARG, FR_INVALID_PARAMETER, 0);
        return ESP_ERR_INVALID_ARG;
    }
        

    static uint8_t raw[FRAME_RAW_MAX_SIZE];
    UINT br = 0;

    memset(out, 0, sizeof(*out));

    FRESULT fr = f_read(&fp, raw, g_frame_size, &br);

    if(fr != FR_OK) {
    ESP_LOGE(TAG, "f_read failed (fr=%d br=%u)", fr, (unsigned)br);
    set_status(st, ESP_FAIL, fr, br);
    return ESP_FAIL;
    }

    if(br == 0) {
    set_status(st, ESP_ERR_FRAME_EOF, FR_OK, br);
    return ESP_ERR_FRAME_EOF;
    }

    if(br != g_frame_size) {
    ESP_LOGE(TAG, "short read (br=%u expected=%u)", (unsigned)br, (unsigned)g_frame_size);
    set_status(st, ESP_FAIL, fr, br);
    return ESP_FAIL;
    }

    uint8_t* p = raw;
    uint32_t sum = 0;

    /* -------- start_time -------- */
    out->timestamp = (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);

    for(int i = 0; i < 4; i++)
        checksum_add_u8(&sum, p[i]);

    p += 4;

    /* -------- fade -------- */
    out->fade = (*p != 0);
    checksum_add_u8(&sum, *p);
    p += 1;

    /* -------- OF GRB (only enabled) -------- */
    for(int ch = 0; ch < LD_BOARD_PCA9955B_CH_NUM; ch++) {
        if(!ch_info_snapshot.i2c_leds[ch])
            continue;

        uint8_t g = p[0];
        uint8_t r = p[1];
        uint8_t b = p[2];

        checksum_add_u8(&sum, g);
        checksum_add_u8(&sum, r);
        checksum_add_u8(&sum, b);

        out->data.pca9955b[ch].g = g;
        out->data.pca9955b[ch].r = r;
        out->data.pca9955b[ch].b = b;

        p += 3;
    }

    /* -------- WS2812B LED strips -------- */
    for(int strip = 0; strip < LD_BOARD_WS2812B_NUM; strip++) {
        uint16_t cnt = ch_info_snapshot.rmt_strips[strip];

        for(uint16_t i = 0; i < cnt; i++) {
            uint8_t g = p[0];
            uint8_t r = p[1];
            uint8_t b = p[2];

            checksum_add_u8(&sum, g);
            checksum_add_u8(&sum, r);
            checksum_add_u8(&sum, b);

            out->data.ws2812b[strip][i].g = g;
            out->data.ws2812b[strip][i].r = r;
            out->data.ws2812b[strip][i].b = b;

            p += 3;
        }
    }

    /* -------- checksum (reserved, consume only) -------- */
    uint32_t read_checksum = 0;
    read_checksum |= (uint32_t)p[0];
    read_checksum |= (uint32_t)p[1] << 8;
    read_checksum |= (uint32_t)p[2] << 16;
    read_checksum |= (uint32_t)p[3] << 24;

    p += CHECKSUM_SIZE;

    if(read_checksum != sum){
        ESP_LOGE(TAG, "checksum mismatch. read=%u calculate=%u", read_checksum, sum);
        set_status(st, ESP_ERR_INVALID_CRC, FR_OK, br);
        return ESP_ERR_INVALID_CRC;
    }

    if((uint32_t)(p - raw) != g_frame_size) {
        ESP_LOGE(TAG, "frame consume mismatch used=%u size=%u", (unsigned)(p - raw), (unsigned)g_frame_size);
        set_status(st, ESP_FAIL, FR_OK, br);
        return ESP_FAIL;
    }

    return ESP_OK;
}
