#pragma once

#include "driver/gptimer.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "player_protocal.h"

enum class MetronomeState {
    UNINIT,
    STOPPED,
    RUNNING,
};

class PlayerMetronome {
  public:
    PlayerMetronome();
    ~PlayerMetronome();

    esp_err_t init(TaskHandle_t task, uint32_t period_us);
    esp_err_t deinit();

    esp_err_t start();
    esp_err_t stop();
    esp_err_t reset();

    esp_err_t set_period_us(uint32_t period_us);

    bool is_running() const;

  private:
    gptimer_handle_t timer = nullptr;
    TaskHandle_t task = nullptr;

    uint32_t period_us = 0;
    MetronomeState state = MetronomeState::UNINIT;
};

enum class ClockState {
    UNINIT,
    STOPPED,
    RUNNING,
    PAUSED,
};

class PlayerClock {
  public:
    PlayerClock();
    ~PlayerClock();

    esp_err_t init(bool with_metronome, TaskHandle_t task, uint32_t period_us);
    esp_err_t deinit();

    esp_err_t start();
    esp_err_t pause();
    esp_err_t reset();

    int64_t now_us() const;

  private:
    ClockState state = ClockState::UNINIT;

    int64_t accumulated_us = 0;
    int64_t last_start_us = 0;

    bool with_metronome = false;
    PlayerMetronome metronome;
};