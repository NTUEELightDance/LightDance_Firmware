#include "state.h"
#include "esp_log.h"

// ================= ErrorState =================

ErrorState& ErrorState::getInstance() {
    static ErrorState s;
    return s;
}

void ErrorState::enter(Player& player) {
#if SHOW_TRANSITION
    ESP_LOGI("state.cpp", "Enter Error!");
#endif

    vTaskDelay(pdMS_TO_TICKS(1000));
    if(player.init_retry_count < 3) {
        player.init_retry_count++;
        player.changeState(ResetState::getInstance());
    }
}

void ErrorState::exit(Player& player) {
    // Do nothing
#if SHOW_TRANSITION
    ESP_LOGI("state.cpp", "Exit Error!");
#endif
}

void ErrorState::handleEvent(Player& player, Event& event) {
    // ignore
}
void ErrorState::update(Player& player) {
    // ignore
}

// ================= ResetState =================

ResetState& ResetState::getInstance() {
    static ResetState s;
    return s;
}

void ResetState::enter(Player& player) {
#if SHOW_TRANSITION
    ESP_LOGI("state.cpp", "Enter Reset!");
#endif

    if(player.performHardwareReset() == ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(1));
        player.changeState(ReadyState::getInstance());
    } else {
        player.changeState(ErrorState::getInstance());
    }
}

void ResetState::exit(Player& player) {
    // Do nothing

#if SHOW_TRANSITION
    ESP_LOGI("state.cpp", "Exit Reset!");
#endif
}

void ResetState::handleEvent(Player& player, Event& event) {
    // ignore
}
void ResetState::update(Player& player) {
    // ignore
}

// ================= ReadyState =================

ReadyState& ReadyState::getInstance() {
    static ReadyState s;
    return s;
}

void ReadyState::enter(Player& player) {
#if SHOW_TRANSITION
    ESP_LOGI("state.cpp", "Enter Ready!");
#endif

    if(player.performHardwareClear() != ESP_OK || player.fillBuffers() != ESP_OK) {
        player.init_retry_count = 0;
        player.changeState(ErrorState::getInstance());
    }

    vTaskDelay(pdMS_TO_TICKS(1));
}

void ReadyState::exit(Player& player) {
    // Do nothing

#if SHOW_TRANSITION
    ESP_LOGI("state.cpp", "Exit Ready!");
#endif
}

void ReadyState::handleEvent(Player& player, Event& event) {
    if(event.type == EVENT_PLAY) {
        player.changeState(PlayingState::getInstance());
    }
    if(event.type == EVENT_TEST) {
        player.changeState(TestState::getInstance());
    }
    if(event.type == EVENT_RESET) {
        player.changeState(ResetState::getInstance());
    }
}
void ReadyState::update(Player& player) {
    // ignore
}

// ================= PlayingState =================

PlayingState& PlayingState::getInstance() {
    static PlayingState s;
    return s;
}

void PlayingState::enter(Player& player) {

#if SHOW_TRANSITION
    ESP_LOGI("state.cpp", "Enter Playing!");
#endif
    player.startTimer(10);
    player.getStartTime();
    player.update();
}

void PlayingState::exit(Player& player) {
    player.stopTimer();

#if SHOW_TRANSITION
    ESP_LOGI("state.cpp", "Exit Playing!");
#endif
}

void PlayingState::handleEvent(Player& player, Event& event) {
    if(event.type == EVENT_PAUSE) {
        player.changeState(PauseState::getInstance());
    }
    if(event.type == EVENT_READY) {
        player.changeState(ReadyState::getInstance());
    }
    if(event.type == EVENT_RESET) {
        player.changeState(ResetState::getInstance());
    }
}
void PlayingState::update(Player& player) {
    player.showFrame();
    if(!player.computeFrame()) {
        player.changeState(ReadyState::getInstance());
    }

#if SHOW_TRANSITION
    ESP_LOGI("state.cpp", "Update!");
#endif
}

// ================= PauseState =================

PauseState& PauseState::getInstance() {
    static PauseState s;
    return s;
}

void PauseState::enter(Player& player) {
#if SHOW_TRANSITION
    ESP_LOGI("state.cpp", "Enter Pause!");
#endif
    player.showFrame();
}
void PauseState::exit(Player& player) {
    // Do nothing
#if SHOW_TRANSITION
    ESP_LOGI("state.cpp", "Exit Pause!");
#endif
}

void PauseState::handleEvent(Player& player, Event& event) {
    if(event.type == EVENT_PLAY) {
        player.changeState(PlayingState::getInstance());
    }
    if(event.type == EVENT_TEST) {
        player.changeState(TestState::getInstance());
    }
    if(event.type == EVENT_READY) {
        player.changeState(ReadyState::getInstance());
    }
    if(event.type == EVENT_RESET) {
        player.changeState(ResetState::getInstance());
    }
}
void PauseState::update(Player& player) {
    // ignore
}

// ================= TestState =================

TestState& TestState::getInstance() {
    static TestState s;
    return s;
}

void TestState::enter(Player& player) {
#if SHOW_TRANSITION
    ESP_LOGI("state.cpp", "Enter Test!");
#endif

    player.startTimer(10);
    player.getStartTime();
    player.update();
}

void TestState::exit(Player& player) {
    player.stopTimer();

#if SHOW_TRANSITION
    ESP_LOGI("state.cpp", "Exit Test!");
#endif
}

void TestState::handleEvent(Player& player, Event& event) {
    if(event.type == EVENT_PAUSE) {
        player.changeState(PauseState::getInstance());
    }
    if(event.type == EVENT_READY) {
        player.changeState(ReadyState::getInstance());
    }
    if(event.type == EVENT_RESET) {
        player.changeState(ResetState::getInstance());
    }
}

void TestState::update(Player& player) {
    player.showFrame();
    player.computeTestFrame();

#if SHOW_TRANSITION
    ESP_LOGI("state.cpp", "Update!");
#endif
}