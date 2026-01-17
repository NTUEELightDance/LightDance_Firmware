#include "framebuffer.h"

#include "algorithm"
#include "esp_log.h"

static const char* TAG = "fb";

static int count = 0;

void swap(table_frame_t*& a, table_frame_t*& b) {
    table_frame_t* tmp = a;
    a = b;
    b = tmp;
}

FrameBuffer::FrameBuffer() {
    current = &frame0;
    next = &frame1;
}
FrameBuffer::~FrameBuffer() {}

esp_err_t FrameBuffer::init() {
    current = &frame0;
    next = &frame1;

    memset(&frame0, 0, sizeof(frame0));
    memset(&frame1, 0, sizeof(frame1));
    memset(&buffer, 0, sizeof(buffer));

    count = 0;
    test_read_frame(current);
    test_read_frame(next);

    return ESP_OK;
}

esp_err_t FrameBuffer::reset() {
    current = &frame0;
    next = &frame1;

    memset(&frame0, 0, sizeof(frame0));
    memset(&frame1, 0, sizeof(frame1));
    memset(&buffer, 0, sizeof(buffer));

    count = 0;
    test_read_frame(current);
    test_read_frame(next);

    return ESP_OK;
}

esp_err_t FrameBuffer::deinit() {
    return ESP_OK;
}

void FrameBuffer::compute(uint64_t time_ms) {
    if(current == nullptr || next == nullptr) {
        ESP_LOGE(TAG, "FrameBuffer not initialized");
        return;
    }

    if(time_ms < current->timestamp) {
        buffer = current->data;
        return;
    }

    while(time_ms >= next->timestamp) {
        std::swap(current, next);
        test_read_frame(next);
        if(next->timestamp <= current->timestamp) {
            ESP_LOGE(TAG, "Non-monotonic timestamp: current=%" PRIu64 ", next=%" PRIu64, current->timestamp, next->timestamp);
            buffer = current->data;
            return;
        }
    }

    uint8_t p = 0;

    const uint64_t t1 = current->timestamp;
    const uint64_t t2 = next->timestamp;

    if(current->fade) {
        if(t2 <= t1) {
            p = 255;
        } else if(time_ms >= t2) {
            p = 255;
        } else {
            const uint64_t dt = time_ms - t1;
            const uint64_t dur = t2 - t1;
            p = (uint8_t)((dt * 255) / dur);
        }
    } else {
        p = (time_ms >= t2) ? 255 : 0;
    }

    for(int ch = 0; ch < WS2812B_NUM; ch++) {
        int len = ch_info.rmt_strips[ch];
        if(len < 0)
            len = 0;
        if(len > WS2812B_MAX_PIXEL_NUM)
            len = WS2812B_MAX_PIXEL_NUM;

        for(int i = 0; i < len; i++) {
            buffer.ws2812b[ch][i] = grb_lerp_hsv_u8(current->data.ws2812b[ch][i], next->data.ws2812b[ch][i], p);
        }
        for(int i = len; i < WS2812B_MAX_PIXEL_NUM; i++) {
            buffer.ws2812b[ch][i] = {0, 0, 0};
        }
    }

    for(int ch = 0; ch < PCA9955B_CH_NUM; ch++) {
        buffer.pca9955b[ch] = grb_lerp_hsv_u8(current->data.pca9955b[ch], next->data.pca9955b[ch], p);
    }
}

frame_data* FrameBuffer::get_buffer() {
    return &buffer;
}

void print_table_frame(const table_frame_t& frame) {
    ESP_LOGI(TAG, "=== table_frame_t ===");
    ESP_LOGI(TAG, "timestamp : %" PRIu64 " ms", frame.timestamp);
    ESP_LOGI(TAG, "fade      : %s", frame.fade ? "true" : "false");
    print_frame_data(frame.data);
    ESP_LOGI(TAG, "=====================");
}

void print_frame_data(const frame_data& data) {
    ESP_LOGI(TAG, "[WS2812]");
    for(int ch = 0; ch < WS2812B_NUM; ch++) {
        int len = ch_info.rmt_strips[ch];

        if(len < 0)
            len = 0;
        if(len > WS2812B_MAX_PIXEL_NUM)
            len = WS2812B_MAX_PIXEL_NUM;

        int dump = (len > 5) ? 5 : len;

        ESP_LOGI(TAG, "  CH %d (len=%d):", ch, len);
        for(int i = 0; i < dump; i++) {
            const grb8_t& p = data.ws2812b[ch][i];
            ESP_LOGI(TAG, "    [%d] G=%u R=%u B=%u", i, p.g, p.r, p.b);
        }
        if(dump < len) {
            ESP_LOGI(TAG, "    ...");
        }
    }

    ESP_LOGI(TAG, "[PCA9955]");
    for(int i = 0; i < PCA9955B_CH_NUM; i++) {
        const grb8_t& p = data.pca9955b[i];
        ESP_LOGI(TAG, "  CH %2d: G=%u R=%u B=%u", i, p.g, p.r, p.b);
    }
}

void FrameBuffer::print_buffer() {
    print_frame_data(buffer);
}

static uint8_t brightness = 31;

static grb8_t red = {.g = 0, .r = brightness, .b = 0};
static grb8_t green = {.g = brightness, .r = 0, .b = 0};
static grb8_t blue = {.g = 0, .r = 0, .b = brightness};
static grb8_t color_pool[3] = {red, green, blue};

void test_read_frame(table_frame_t* p) {
    p->timestamp = count * 2000;
    p->fade = true;
    for(int ch_idx = 0; ch_idx < WS2812B_NUM; ch_idx++) {
        for(int i = 0; i < ch_info.rmt_strips[ch_idx]; i++) {
            p->data.ws2812b[ch_idx][i] = grb_lerp_hsv_u8(color_pool[count % 3], color_pool[(count + 1) % 3], i * 255 / ch_info.rmt_strips[ch_idx]);
        }
    }
    for(int ch_idx = 0; ch_idx < PCA9955B_CH_NUM; ch_idx++) {
        if(ch_info.i2c_leds[ch_idx]) {
            p->data.pca9955b[ch_idx] = color_pool[count % 3];
        }
    }
    count++;
}