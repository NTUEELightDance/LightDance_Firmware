#include "player_state.hpp"

#include "esp_log.h"

// ================= ReadyState =================

ReadyState& ReadyState::getInstance() {
    static ReadyState s;
    return s;
}

void ReadyState::enter(Player& player) {
#if SHOW_TRANSITION
    ESP_LOGI("state.cpp", "Enter Ready!");
#endif

    player.resetPlayback();
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
        player.testPlayback(event.test_data.r, event.test_data.g, event.test_data.b);
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
    player.startPlayback();
}

void PlayingState::exit(Player& player) {

#if SHOW_TRANSITION
    ESP_LOGI("state.cpp", "Exit Playing!");
#endif
}

void PlayingState::handleEvent(Player& player, Event& event) {
    if(event.type == EVENT_PAUSE) {
        player.changeState(PauseState::getInstance());
    }
    if(event.type == EVENT_RESET) {
        player.changeState(ReadyState::getInstance());
    }
}

void PlayingState::update(Player& player) {
#if SHOW_TRANSITION
    ESP_LOGI("state.cpp", "Update!");
#endif
    player.updatePlayback();
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
    player.pausePlayback();
}
void PauseState::exit(Player& player) {
#if SHOW_TRANSITION
    ESP_LOGI("state.cpp", "Exit Pause!");
#endif
}

void PauseState::handleEvent(Player& player, Event& event) {
    if(event.type == EVENT_PLAY) {
        player.changeState(PlayingState::getInstance());
    }
    if(event.type == EVENT_RESET) {
        player.changeState(ReadyState::getInstance());
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
}

void TestState::exit(Player& player) {

#if SHOW_TRANSITION
    ESP_LOGI("state.cpp", "Exit Test!");
#endif
}

void TestState::handleEvent(Player& player, Event& event) {
    if(event.type == EVENT_TEST) {
        player.testPlayback(event.test_data.r, event.test_data.g, event.test_data.b);
    }

    if(event.type == EVENT_RESET) {
        player.changeState(ReadyState::getInstance());
    }
}

void TestState::update(Player& player) {

#if SHOW_TRANSITION
    ESP_LOGI("state.cpp", "Update!");
#endif
}