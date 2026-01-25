#pragma once

#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "LedController.hpp"
#include "framebuffer.hpp"
#include "player_clock.h"
#include "player_protocal.h"

#include "readframe.h"

#define SHOW_TRANSITION 0

class Player {
  public:
    // ===== Singleton =====

    static Player& getInstance();

    // ===== Lifecycle =====

    esp_err_t init();
    esp_err_t deinit();

    // ===== External Commands (thread-safe) =====

    esp_err_t play();
    esp_err_t pause();
    esp_err_t reset();
    esp_err_t release();
    esp_err_t load();
    esp_err_t test(uint8_t, uint8_t, uint8_t);
    esp_err_t exit();

  private:
    // ===== Called by State =====

    esp_err_t startPlayback();
    esp_err_t pausePlayback();
    esp_err_t resetPlayback();
    esp_err_t updatePlayback();
    esp_err_t testPlayback(uint8_t, uint8_t, uint8_t);

    // ===== FSM =====

    enum class PlayerState {
        UNLOADED,
        READY,
        PLAYING,
        PAUSE,
        TEST,
    };

    const char* getStateName(PlayerState state);
    void switchState(PlayerState newState);  //  enter/exit
    void processEvent(Event& e);             //  handleEvent (switch-case)
    void updateState();                      //  update

    // ===== Resource Management =====

    esp_err_t acquireResources();
    esp_err_t releaseResources();

  private:
    Player();
    ~Player();

    // ===== RTOS =====

    esp_err_t createTask();
    static void taskEntry(void* pvParameters);
    void Loop();

    esp_err_t sendEvent(Event& e);  // always safe, returns error

  private:
    // ===== FSM =====

    PlayerState m_state = PlayerState::UNLOADED;

    // ===== Resources =====

    bool resources_acquired = false;
    PlayerClock clock;
    LedController controller;
    FrameBuffer fb;

    // ===== RTOS Objects =====

    bool taskAlive = false;
    TaskHandle_t taskHandle = nullptr;
    QueueHandle_t eventQueue = nullptr;
};

void console_test();