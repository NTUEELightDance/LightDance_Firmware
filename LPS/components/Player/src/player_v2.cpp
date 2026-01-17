#include "player_v2.hpp"
#include "player_state.hpp"

#include "esp_check.h"
#include "esp_log.h"

static const char* TAG = "Player";

/* ================= Singleton ================= */

Player& Player::getInstance() {
    static Player instance;
    return instance;
}

Player::Player() = default;
Player::~Player() = default;

/* ================= Public lifecycle ================= */

esp_err_t Player::init() {
    ESP_RETURN_ON_FALSE(!taskAlive, ESP_ERR_INVALID_STATE, TAG, "player already started");

    currentState = &ReadyState::getInstance();
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

esp_err_t Player::reset() {
    Event e{};
    e.type = EVENT_RESET;
    return sendEvent(e);
}

esp_err_t Player::test(uint8_t r, uint8_t g, uint8_t b) {
    Event e{};
    e.type = EVENT_TEST;
    e.test_data.r = r;
    e.test_data.g = g;
    e.test_data.b = b;
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
    clock.pause();
    clock.reset();
    fb.reset();

    controller.fill(0, 0, 0);
    controller.show();

    return ESP_OK;
}

esp_err_t Player::updatePlayback() {
    const uint64_t time_ms = clock.now_us() / 1000;
    fb.compute(time_ms);

    frame_data* buf = fb.get_buffer();

    for(int i = 0; i < WS2812B_NUM; i++) {
        controller.write_buffer(i, (uint8_t*)buf->ws2812b[i]);
    }

    for(int i = 0; i < PCA9955B_CH_NUM; i++) {
        controller.write_buffer(i + WS2812B_NUM, (uint8_t*)&buf->pca9955b[i]);
    }

    controller.show();
    return ESP_OK;
}

esp_err_t Player::testPlayback(uint8_t r, uint8_t g, uint8_t b) {
    controller.fill(r, g, b);
    controller.show();
    return ESP_OK;
}

/* ================= FSM ================= */

void Player::changeState(State& newState) {
    currentState->exit(*this);
    currentState = &newState;
    currentState->enter(*this);
}

/* ================= RTOS ================= */

esp_err_t Player::createTask() {
    BaseType_t res = xTaskCreatePinnedToCore(Player::taskEntry, "PlayerTask", 8192, NULL, 5, &taskHandle, 0);
    ESP_RETURN_ON_FALSE(res == pdPASS, ESP_FAIL, TAG, "create task failed");

    taskAlive = true;
    return ESP_OK;
}

void Player::taskEntry(void* pvParameters) {
    Player& p = Player::getInstance();

    if(p.acquireResources() != ESP_OK) {
        ESP_LOGE(TAG, "resource acquire failed");
        p.taskAlive = false;
        vTaskDelete(nullptr);
    }

    p.Loop();
}

void Player::Loop() {

    currentState->enter(*this);

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
                currentState->handleEvent(*this, e);
            }
        }

        if(running && (ulNotifiedValue & NOTIFICATION_UPDATE)) {
            currentState->update(*this);
        }
    }

    releaseResources();
    taskAlive = false;
    ESP_LOGI(TAG, "player task exit");
    vTaskDelete(NULL);
}

/* ================= Event sending ================= */

esp_err_t Player::sendEvent(Event& event) {
    ESP_RETURN_ON_FALSE(taskAlive && eventQueue != nullptr, ESP_ERR_INVALID_STATE, TAG, "player not ready");
    ESP_RETURN_ON_FALSE(xQueueSend(eventQueue, &event, 0) == pdTRUE, ESP_ERR_TIMEOUT, TAG, "event queue full");
    xTaskNotify(taskHandle, NOTIFICATION_EVENT, eSetBits);
    return ESP_OK;
}

esp_err_t Player::acquireResources() {
    if(resources_acquired) {
        return ESP_OK;
    }

    /* ---- hardware config (temporary placement) ---- */
    for(int i = 0; i < WS2812B_NUM; i++) {
        ch_info.rmt_strips[i] = WS2812B_MAX_PIXEL_NUM;
    }
    for(int i = 0; i < PCA9955B_CH_NUM; i++) {
        ch_info.i2c_leds[i] = 1;
    }

    eventQueue = xQueueCreate(50, sizeof(Event));
    ESP_RETURN_ON_FALSE(eventQueue != nullptr, ESP_ERR_NO_MEM, TAG, "queue alloc failed");

    ESP_RETURN_ON_ERROR(controller.init(), TAG, "controller init failed");
    ESP_RETURN_ON_ERROR(fb.init(), TAG, "framebuffer init failed");
    ESP_RETURN_ON_ERROR(clock.init(true, taskHandle, 1000000 / 40), TAG, "clock init failed");

    resources_acquired = true;
    return ESP_OK;
}

esp_err_t Player::releaseResources() {
    if(!resources_acquired) {
        return ESP_OK;
    }

    clock.deinit();
    fb.deinit();
    controller.deinit();

    if(eventQueue) {
        vQueueDelete(eventQueue);
        eventQueue = nullptr;
    }

    resources_acquired = false;
    return ESP_OK;
}
