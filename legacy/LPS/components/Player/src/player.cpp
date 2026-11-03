#include "player.hpp"
#include "bt_receiver.h"

#include "esp_check.h"
#include "esp_log.h"

#include "freertos/queue.h"

static const char* TAG = "PLAYER";

/* ================= Singleton ================= */

Player& Player::getInstance() {
    static Player instance;
    return instance;
}

Player::Player() = default;
Player::~Player() = default;

/* ================= Public lifecycle ================= */

esp_err_t Player::init() {
    ESP_RETURN_ON_FALSE(!taskAlive, ESP_ERR_INVALID_STATE, TAG, "initialization rejected: player task is already running");
    return createTask();
}

esp_err_t Player::deinit() {
    Event e{};
    e.type = EVENT_EXIT;
    return sendEvent(e);
}

/* ================= External commands ================= */

esp_err_t Player::play() {
    Event e{};
    e.type = EVENT_PLAY;
    return sendEvent(e);
}

esp_err_t Player::pause() {
    Event e{};
    e.type = EVENT_PAUSE;
    return sendEvent(e);
}

esp_err_t Player::stop() {
    Event e{};
    e.type = EVENT_STOP;
    return sendEvent(e);
}

esp_err_t Player::release() {
    Event e{};
    e.type = EVENT_RELEASE;
    return sendEvent(e);
}

// esp_err_t Player::load() {
//     Event e{};
//     e.type = EVENT_LOAD;
//     return sendEvent(e);
// }

esp_err_t Player::test() {
    Event e{};
    e.type = EVENT_TEST;
    e.test_data.mode = BREATH_RGB;

    return sendEvent(e);
}
esp_err_t Player::test(uint8_t r, uint8_t g, uint8_t b) {
    Event e{};
    e.type = EVENT_TEST;
    e.test_data.mode = SOLID_RGB;

    e.test_data.r = r;
    e.test_data.g = g;
    e.test_data.b = b;
    return sendEvent(e);
}

esp_err_t Player::exit() {
    Event e{};
    e.type = EVENT_EXIT;
    return sendEvent(e);
}

esp_err_t Player::set_time_us(uint32_t start_time_us) {
    ESP_RETURN_ON_ERROR(clock.set_time_us(start_time_us), TAG, "clock seek failed: target_us=%lu", (unsigned long)start_time_us);
    ESP_RETURN_ON_ERROR(fb.seek(start_time_us / 1000), TAG, "framebuffer seek failed: target_ms=%lu", (unsigned long)(start_time_us / 1000));

    FbComputeStatus fb_status = fb.compute(start_time_us / 1000);
    if(fb_status == FbComputeStatus::ERROR_GENERAL) {
        ESP_LOGE(TAG, "framebuffer could not compute the seek target: target_us=%lu status=general_error", (unsigned long)start_time_us);
        return ESP_FAIL;
    }
    if(fb_status == FbComputeStatus::ERROR_CRITICAL) {
        ESP_LOGE(TAG, "framebuffer could not compute the seek target: target_us=%lu status=critical_error", (unsigned long)start_time_us);
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t Player::seek(uint32_t start_time_us) {
    Event e;
    e.type = EVENT_SEEK;
    e.data = start_time_us;
    return sendEvent(e);
}

/* ================= Playback control (called by State) ================= */

esp_err_t Player::startPlayback() {
    return clock.start();
}

esp_err_t Player::pausePlayback() {
    return clock.pause();
}

esp_err_t Player::resetPlayback() {
    ESP_RETURN_ON_ERROR(clock.pause(), TAG, "playback reset failed while pausing the clock");
    ESP_RETURN_ON_ERROR(clock.reset(), TAG, "playback reset failed while resetting the clock");
    ESP_RETURN_ON_ERROR(fb.reset(), TAG, "playback reset failed while resetting the framebuffer");
    controller.fill(GRB_BLACK);
    controller.show();

    return ESP_OK;
}

esp_err_t Player::updatePlayback() {
    const uint64_t time_ms = clock.now_us() / 1000;

    FbComputeStatus fb_status = fb.compute(time_ms);
    if(fb_status == FbComputeStatus::ERROR_GENERAL) {
        ESP_LOGE(TAG, "playback stopped after a recoverable framebuffer error: playback_ms=%llu", (unsigned long long)time_ms);
        Event e{};
        e.type = EVENT_STOP;
        ESP_RETURN_ON_ERROR(sendEvent(e), TAG, "stop event enqueue failed after a framebuffer error");
        // return ESP_FAIL;
    } else if(fb_status == FbComputeStatus::ERROR_CRITICAL) {
        ESP_LOGE(TAG, "playback stopped after a critical framebuffer error: playback_ms=%llu", (unsigned long long)time_ms);
        Event e{};
        e.type = EVENT_STOP;
        ESP_RETURN_ON_ERROR(sendEvent(e), TAG, "stop event enqueue failed after a critical framebuffer error");
    }

    frame_data* buf = fb.get_buffer();
    controller.write_frame(buf);

    // print_frame_data(*buf);

    controller.show();

    if(fb_status == FbComputeStatus::EOF_REACHED) {
        Event e{};
        e.type = EVENT_STOP;
        ESP_RETURN_ON_ERROR(sendEvent(e), TAG, "stop event enqueue failed at end of playback");
    }

    return ESP_OK;
}

esp_err_t Player::testPlayback(TestData data) {
    if(data.mode == SOLID_RGB) {
        fb.set_test_mode(FbTestMode::SOLID);
        fb.set_test_color(grb8(data.r, data.g, data.b));
    }
    if(data.mode == BREATH_RGB) {
        fb.set_test_mode(FbTestMode::BREATH);
    }

    clock.start();

    return ESP_OK;
}

/* ================= RTOS ================= */

esp_err_t Player::createTask() {
    eventQueue = xQueueCreate(LD_CFG_PLAYER_EVENT_QUEUE_LEN, sizeof(Event));
    BaseType_t res = xTaskCreatePinnedToCore(Player::taskEntry, LD_CFG_PLAYER_TASK_NAME, LD_CFG_PLAYER_TASK_STACK_SIZE, NULL, LD_CFG_PLAYER_TASK_PRIORITY, &taskHandle, LD_CFG_PLAYER_TASK_CORE_ID);

    ESP_RETURN_ON_FALSE(res == pdPASS,
                        ESP_FAIL,
                        TAG,
                        "player task creation failed: name=%s stack_size=%u priority=%u core=%d",
                        LD_CFG_PLAYER_TASK_NAME,
                        (unsigned)LD_CFG_PLAYER_TASK_STACK_SIZE,
                        (unsigned)LD_CFG_PLAYER_TASK_PRIORITY,
                        LD_CFG_PLAYER_TASK_CORE_ID);
    taskAlive = true;
    ESP_LOGI(TAG, "player task started");
    return ESP_OK;
}

void Player::taskEntry(void* pvParameters) {

    Player& p = Player::getInstance();

    Event bootEvent;
    bootEvent.type = EVENT_LOAD;
    p.processEvent(bootEvent);  // auto-load on start

    p.Loop();
}

void Player::Loop() {
    Event e{};
    uint32_t ulNotifiedValue;
    bool running = true;

    while(running) {
        xTaskNotifyWait(0, UINT32_MAX, &ulNotifiedValue, portMAX_DELAY);

        if(ulNotifiedValue & NOTIFICATION_EVENT) {
            while(xQueueReceive(eventQueue, &e, 0) == pdTRUE) {
                if(e.type == EVENT_EXIT) {
                    running = false;
                    break;
                }
                processEvent(e);
            }
        }

        if(running && (ulNotifiedValue & NOTIFICATION_UPDATE)) {
            updateState();
        }
    }

    releaseResources();
    if(eventQueue) {
        vQueueDelete(eventQueue);
        eventQueue = nullptr;
    }
    taskAlive = false;
    ESP_LOGI(TAG, "player task stopped");
    vTaskDelete(NULL);
}

/* ================= Event sending ================= */

esp_err_t Player::sendEvent(Event& event) {
    ESP_RETURN_ON_FALSE(taskAlive && eventQueue != nullptr, ESP_ERR_INVALID_STATE, TAG, "event rejected: player task is not ready event_type=%d", (int)event.type);
    ESP_RETURN_ON_FALSE(xQueueSend(eventQueue, &event, 0) == pdTRUE, ESP_ERR_TIMEOUT, TAG, "event queue full: event_type=%d queue_length=%u", (int)event.type, (unsigned)LD_CFG_PLAYER_EVENT_QUEUE_LEN);
    xTaskNotify(taskHandle, NOTIFICATION_EVENT, eSetBits);
    return ESP_OK;
}

esp_err_t Player::acquireResources() {
    if(resources_acquired) {
        return ESP_OK;
    }

    ESP_RETURN_ON_FALSE(eventQueue != nullptr, ESP_ERR_NO_MEM, TAG, "resource acquisition failed: event queue is NULL");
    ESP_RETURN_ON_ERROR(controller.init(), TAG, "resource acquisition failed: LED controller initialization error");
    ESP_RETURN_ON_ERROR(fb.init(), TAG, "resource acquisition failed: framebuffer initialization error");
    ESP_RETURN_ON_FALSE(LD_CFG_PLAYER_FPS > 0, ESP_ERR_INVALID_ARG, TAG, "resource acquisition failed: configured FPS must be greater than zero");
    ESP_RETURN_ON_ERROR(clock.init(true, taskHandle, 1000000 / LD_CFG_PLAYER_FPS), TAG, "resource acquisition failed: clock initialization error");

    resources_acquired = true;
    return ESP_OK;
}

esp_err_t Player::releaseResources() {
    if(!resources_acquired) {
        return ESP_OK;
    }

    ESP_RETURN_ON_ERROR(clock.deinit(), TAG, "resource release failed: clock deinitialization error");
    ESP_RETURN_ON_ERROR(fb.deinit(), TAG, "resource release failed: framebuffer deinitialization error");
    ESP_RETURN_ON_ERROR(controller.deinit(), TAG, "resource release failed: LED controller deinitialization error");

    resources_acquired = false;
    return ESP_OK;
}
