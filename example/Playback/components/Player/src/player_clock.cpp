#include "player_clock.h"

#include "esp_check.h"
#include "esp_err.h"

static const char* TAG = "PlayerClock";

PlayerMetronome::PlayerMetronome() {}

PlayerMetronome::~PlayerMetronome() {
    deinit();
}

static bool IRAM_ATTR timer_on_alarm_cb(gptimer_handle_t timer, const gptimer_alarm_event_data_t* edata, void* user_ctx) {
    TaskHandle_t task = static_cast<TaskHandle_t>(user_ctx);
    BaseType_t hp_task_woken = pdFALSE;

    if(task) {
        xTaskNotifyFromISR(task, NOTIFICATION_UPDATE, eSetBits, &hp_task_woken);
    }

    return hp_task_woken == pdTRUE;
}

esp_err_t PlayerMetronome::init(TaskHandle_t _task, uint32_t _period_us) {
    if(state != MetronomeState::UNINIT) {
        return ESP_OK;
    }

    ESP_RETURN_ON_FALSE(_task != nullptr && _period_us > 0, ESP_ERR_INVALID_ARG, TAG, "invalid task or period");

    task = _task;
    period_us = _period_us;

    gptimer_config_t timer_config = {.clk_src = GPTIMER_CLK_SRC_DEFAULT,  // Select the default clock source
                                     .direction = GPTIMER_COUNT_UP,       // Counting direction is up
                                     .resolution_hz = 1 * 1000 * 1000,    // Resolution is 1 MHz, i.e., 1 tick equals 1 microsecond

                                     .intr_priority = 0,
                                     .flags = {.intr_shared = 0, .allow_pd = 0, .backup_before_sleep = 0}};

    esp_err_t ret;
    ret = gptimer_new_timer(&timer_config, &timer);
    ESP_RETURN_ON_ERROR(ret, TAG, "new timer failed");

    gptimer_event_callbacks_t cbs = {
        .on_alarm = timer_on_alarm_cb,  // Call the user callback function when the alarm event occurs
    };
    ret = gptimer_register_event_callbacks(timer, &cbs, task);
    if(ret != ESP_OK) {
        deinit();
        return ret;
    }

    gptimer_alarm_config_t alarm_cfg = {
        .alarm_count = period_us,
        .reload_count = 0,
        .flags =
            {
                .auto_reload_on_alarm = true,
            },
    };

    ret = gptimer_set_alarm_action(timer, &alarm_cfg);
    if(ret != ESP_OK) {
        deinit();
        return ret;
    }

    ret = gptimer_enable(timer);
    if(ret != ESP_OK) {
        deinit();
        return ret;
    }

    state = MetronomeState::STOPPED;
    return ESP_OK;
}

esp_err_t PlayerMetronome::deinit() {
    if(state == MetronomeState::UNINIT && timer == nullptr) {
        return ESP_OK;
    }

    stop();

    if(timer) {
        gptimer_disable(timer);
        gptimer_del_timer(timer);
        timer = nullptr;
    }

    task = nullptr;
    period_us = 0;
    state = MetronomeState::UNINIT;

    return ESP_OK;
}

esp_err_t PlayerMetronome::start() {
    ESP_RETURN_ON_FALSE(state != MetronomeState::UNINIT, ESP_ERR_INVALID_STATE, TAG, "start before init");

    if(state == MetronomeState::RUNNING) {
        return ESP_OK;
    }

    esp_err_t ret = gptimer_start(timer);
    ESP_RETURN_ON_ERROR(ret, TAG, "gptimer_start failed");

    state = MetronomeState::RUNNING;
    return ESP_OK;
}

esp_err_t PlayerMetronome::stop() {
    if(state != MetronomeState::RUNNING) {
        return ESP_OK;
    }

    esp_err_t ret = gptimer_stop(timer);
    ESP_RETURN_ON_ERROR(ret, TAG, "gptimer_stop failed");

    state = MetronomeState::STOPPED;

    return ESP_OK;
}

esp_err_t PlayerMetronome::reset() {
    ESP_RETURN_ON_FALSE(state != MetronomeState::UNINIT, ESP_ERR_INVALID_STATE, TAG, "reset before init");

    stop();
    gptimer_set_raw_count(timer, 0);

    return ESP_OK;
}

esp_err_t PlayerMetronome::set_period_us(uint32_t new_period_us) {
    ESP_RETURN_ON_FALSE(state != MetronomeState::UNINIT, ESP_ERR_INVALID_STATE, TAG, "set_period before init");
    ESP_RETURN_ON_FALSE(new_period_us > 0, ESP_ERR_INVALID_ARG, TAG, "invalid period");

    bool was_running = (state == MetronomeState::RUNNING);

    if(was_running) {
        esp_err_t ret = stop();
        if(ret != ESP_OK) {
            return ret;
        }
    }

    period_us = new_period_us;

    gptimer_alarm_config_t alarm_config = {
        .alarm_count = period_us,
        .reload_count = 0,
        .flags =
            {
                .auto_reload_on_alarm = true,
            },
    };

    esp_err_t ret = gptimer_set_alarm_action(timer, &alarm_config);
    ESP_RETURN_ON_ERROR(ret, TAG, "set alarm failed");

    if(was_running) {
        ret = start();
        ESP_RETURN_ON_ERROR(ret, TAG, "restart failed");
    }

    return ESP_OK;
}

PlayerClock::PlayerClock() {
    accumulated_us = 0;
    last_start_us = 0;
    with_metronome = false;
    state = ClockState::UNINIT;
}
PlayerClock::~PlayerClock() {
    deinit();
}

esp_err_t PlayerClock::init(bool _with_metronome, TaskHandle_t task, uint32_t period_us) {
    if(state != ClockState::UNINIT) {
        return ESP_OK;
    }

    accumulated_us = 0;
    last_start_us = 0;
    with_metronome = _with_metronome;

    if(with_metronome) {
        ESP_RETURN_ON_FALSE(task != nullptr && period_us > 0, ESP_ERR_INVALID_ARG, TAG, "invalid metronome args");

        esp_err_t ret = metronome.init(task, period_us);
        if(ret != ESP_OK) {
            metronome.deinit();
            return ret;
        }
    }

    state = ClockState::STOPPED;

    return ESP_OK;
}

esp_err_t PlayerClock::deinit() {
    if(state == ClockState::UNINIT) {
        return ESP_OK;
    }

    if(with_metronome) {
        metronome.deinit();
    }
    accumulated_us = 0;
    last_start_us = 0;
    with_metronome = false;
    state = ClockState::UNINIT;

    return ESP_OK;
}

esp_err_t PlayerClock::start() {
    ESP_RETURN_ON_FALSE(state != ClockState::UNINIT, ESP_ERR_INVALID_STATE, TAG, "start before init");

    if(state == ClockState::RUNNING) {
        return ESP_OK;
    }

    last_start_us = esp_timer_get_time();

    if(with_metronome) {
        esp_err_t ret = metronome.start();
        if(ret != ESP_OK) {
            return ret;
        }
    }

    state = ClockState::RUNNING;

    return ESP_OK;
}

esp_err_t PlayerClock::pause() {
    ESP_RETURN_ON_FALSE(state != ClockState::UNINIT, ESP_ERR_INVALID_STATE, TAG, "pause before init");
    if(state != ClockState::RUNNING) {
        return ESP_OK;
    }

    int64_t now = esp_timer_get_time();
    accumulated_us += (now - last_start_us);

    if(with_metronome) {
        metronome.stop();
    }

    state = ClockState::PAUSED;
    return ESP_OK;
}

esp_err_t PlayerClock::reset() {
    ESP_RETURN_ON_FALSE(state != ClockState::UNINIT, ESP_ERR_INVALID_STATE, TAG, "reset before init");

    accumulated_us = 0;

    if(state == ClockState::RUNNING) {
        last_start_us = esp_timer_get_time();
    }

    if(with_metronome) {
        metronome.reset();
    }

    state = ClockState::STOPPED;

    return ESP_OK;
}

int64_t PlayerClock::now_us() const {
    if(state == ClockState::UNINIT) {
        return 0;
    }

    if(state != ClockState::RUNNING) {
        return accumulated_us;
    }

    int64_t now = esp_timer_get_time();
    return accumulated_us + (now - last_start_us);
}
