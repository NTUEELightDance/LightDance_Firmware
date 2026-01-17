#include "player_state.hpp"

#include "esp_log.h"

static const char* TAG = "state.cpp";
const char* getEventName(int type) {
    switch (type) {
        case EVENT_PLAY:    return "PLAY";
        case EVENT_PAUSE:   return "PAUSE";
        case EVENT_RESET:   return "RESET";
        case EVENT_RELEASE: return "RELEASE";
        case EVENT_LOAD:    return "LOAD";
        case EVENT_TEST:    return "TEST";
        case EVENT_EXIT:    return "EXIT";
        default:            return "UNKNOWN";
    }
}

// ================= UnloadedState =================

UnloadedState& UnloadedState::getInstance() {
    static UnloadedState s;
    return s;
}

void UnloadedState::enter(Player& player) {
#if SHOW_TRANSITION
    ESP_LOGI("state.cpp", "Enter Unloaded!");
#endif
    if(player.releaseResources() == ESP_OK) {
        ESP_LOGI(TAG, "resources released");
    } else {
        ESP_LOGE(TAG, "resources release failed");
    }
}

void UnloadedState::handleEvent(Player& player, Event& event) {
    if(event.type == EVENT_LOAD) {
        if(player.acquireResources() == ESP_OK) {
            player.changeState(ReadyState::getInstance());
        } else {
            ESP_LOGE(TAG, "resource acquire failed");
        }
    }
    else{
        ESP_LOGW(TAG, "UnloadedState: ignoring event %s", getEventName(event.type));
    }
}

void UnloadedState::exit(Player& player) {
    // Do nothing

#if SHOW_TRANSITION
    ESP_LOGI("state.cpp", "Exit Unloaded!");
#endif
}

void UnloadedState::update(Player& player) {
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
    if(player.resetPlayback() == ESP_OK) {
        ESP_LOGI(TAG, "ready to play");
    } else {
        ESP_LOGE(TAG, "playback reset failed, enter UnloadedState");
        player.changeState(UnloadedState::getInstance());
    }
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
    else if(event.type == EVENT_TEST) {
        player.changeState(TestState::getInstance());
        player.testPlayback(event.test_data.r, event.test_data.g, event.test_data.b);
    }
    else if(event.type == EVENT_RELEASE) {
        player.changeState(UnloadedState::getInstance());
    }
    else{
        ESP_LOGW(TAG, "ReadyState: ignoring event %s", getEventName(event.type));
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
    else if(event.type == EVENT_RESET) {
        player.changeState(ReadyState::getInstance());
    }
    else if(event.type == EVENT_RELEASE) {
        player.changeState(UnloadedState::getInstance());
    }
    else{
        ESP_LOGW(TAG, "PlayingState: ignoring event %s", getEventName(event.type));
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
    else if(event.type == EVENT_RESET) {
        player.changeState(ReadyState::getInstance());
    }
    else if(event.type == EVENT_RELEASE) {
        player.changeState(UnloadedState::getInstance());
    }
    else{
        ESP_LOGW(TAG, "PauseState: ignoring event %s", getEventName(event.type));
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
    else if(event.type == EVENT_RESET) {
        player.changeState(ReadyState::getInstance());
    }
    else if(event.type == EVENT_RELEASE) {
        player.changeState(UnloadedState::getInstance());
    }
    else{
        ESP_LOGW(TAG, "TestState: ignoring event %s", getEventName(event.type));
    }
}

void TestState::update(Player& player) {

#if SHOW_TRANSITION
    ESP_LOGI("state.cpp", "Update!");
#endif
}