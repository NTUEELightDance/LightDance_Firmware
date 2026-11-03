#pragma once

#include "driver/gptimer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "LedController_v2.hpp"
#include "framebuffer.h"

typedef enum {
    EVENT_PLAY,
    EVENT_TEST,
    EVENT_PAUSE,
    EVENT_RESET,
    EVENT_READY,
    EVENT_UPDATE,
} event_t;

struct Event {
    event_t type;
    uint32_t data;
};

class State;
class ErrorState;
class ResetState;
class ReadyState;
class PlayingState;
class PauseState;
class TestState;

class Player {
  public:
    static Player& getInstance();

    Player(const Player&) = delete;
    void operator=(const Player&) = delete;

    void start();

    void sendEvent(Event& event);

    TaskHandle_t& getTaskHandle();

  private:
    Player();

    // ================= Finite State Machine =================

    friend class ErrorState;
    friend class ResetState;
    friend class ReadyState;
    friend class PlayingState;
    friend class PauseState;
    friend class TestState;

    State* currentState;
    void update();
    void handleEvent(Event& event);
    void changeState(State& newState);

    struct {
        bool Timer;
        bool Drivers;
        bool Buffers;
    } isHardwareInitialized;

    int init_retry_count = 0;

    // ================= Resources =================

    gptimer_handle_t gptimer;
    LedController controller;
    FrameBuffer fb;

    TaskHandle_t taskHandle;
    QueueHandle_t eventQueue;

    // ================= Task Managment =================

    esp_err_t createTask();
    static void taskEntry(void* pvParameters);
    void Loop();

    // ================= Timer Function Implementation =================

    esp_err_t initTimer();
    esp_err_t deinitTimer();
    esp_err_t clearTimer();
    void startTimer(int fps);
    void stopTimer();

    uint64_t playing_start_time;

    // ================= Driver Function Implementation =================

    esp_err_t initDrivers();
    esp_err_t deinitDrivers();
    esp_err_t clearDrivers();

    // ================= Frame Buffer Management =================

    esp_err_t allocateBuffers();
    esp_err_t freeBuffers();
    esp_err_t clearBuffers();
    esp_err_t fillBuffers();

    void buffersToController();
    void getStartTime();
    bool computeFrame();
    void computeTestFrame();
    void showFrame();

    // ================= Hardware Reset / Clear =================

    esp_err_t performHardwareReset();
    esp_err_t performHardwareClear();
};
