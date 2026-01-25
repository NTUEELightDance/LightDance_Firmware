#include "esp_log.h"
#include "player.hpp"

static const char* TAG = "Player_fsm.cpp";

const char* getEventName(int type) {
    switch(type) {
        case EVENT_PLAY:
            return "PLAY";
        case EVENT_PAUSE:
            return "PAUSE";
        case EVENT_RESET:
            return "RESET";
        case EVENT_RELEASE:
            return "RELEASE";
        case EVENT_LOAD:
            return "LOAD";
        case EVENT_TEST:
            return "TEST";
        case EVENT_EXIT:
            return "EXIT";
        default:
            return "UNKNOWN";
    }
}
const char* Player::getStateName(PlayerState state) {
    switch(state) {
        case PlayerState::UNLOADED:
            return "UNLOADED";
        case PlayerState::READY:
            return "READY";
        case PlayerState::PLAYING:
            return "PLAYING";
        case PlayerState::PAUSE:
            return "PAUSED";
        case PlayerState::TEST:
            return "TEST";
        default:
            return "UNKNOWN";
    }
}

// Enter/Exit State

void Player::switchState(PlayerState newState) {

    ESP_LOGI(TAG, "State Transition: %s -> %s", getStateName(m_state), getStateName(newState));

    // Exit
    switch(m_state) {
        default:
#if SHOW_TRANSITION
            ESP_LOGI("state.cpp", "Exit state: %s", getStateName(m_state));
#endif
            break;
    }

    m_state = newState;

    // Enter
    switch(m_state) {
        case PlayerState::UNLOADED: {
#if SHOW_TRANSITION
            ESP_LOGI("state.cpp", "Enter Unloaded!");
#endif
            if(releaseResources() == ESP_OK) {
                ESP_LOGI(TAG, "resources released");
            } else {
                ESP_LOGE(TAG, "resources release failed");
            }
        } break;

        case PlayerState::READY: {
#if SHOW_TRANSITION
            ESP_LOGI("state.cpp", "Enter Ready!");
#endif
            if(resetPlayback() == ESP_OK) {
                ESP_LOGI(TAG, "ready to play");
            } else {
                ESP_LOGE(TAG, "playback reset failed, enter UnloadedState");
                switchState(PlayerState::UNLOADED);
            }
        } break;

        case PlayerState::PLAYING: {
#if SHOW_TRANSITION
            ESP_LOGI("state.cpp", "Enter Playing!");
#endif
            startPlayback();
        } break;

        case PlayerState::PAUSE: {
#if SHOW_TRANSITION
            ESP_LOGI("state.cpp", "Enter Pause!");
#endif
            pausePlayback();
        } break;

        case PlayerState::TEST: {
#if SHOW_TRANSITION
            ESP_LOGI("state.cpp", "Enter Test!");
#endif
        } break;

        default:
            break;
    }
}

// Handle Event

void Player::processEvent(Event& e) {

    switch(m_state) {
        case PlayerState::UNLOADED:
            if(e.type == EVENT_LOAD) {
                if(acquireResources() == ESP_OK)
                    switchState(PlayerState::READY);
                else
                    ESP_LOGE(TAG, "resource acquire failed");
            } else
                ESP_LOGW(TAG, "UnloadedState: ignoring event %s", getEventName(e.type));
            break;

        case PlayerState::READY:
            if(e.type == EVENT_PLAY)
                switchState(PlayerState::PLAYING);
            else if(e.type == EVENT_RELEASE)
                switchState(PlayerState::UNLOADED);
            else if(e.type == EVENT_TEST) {
                testPlayback(e.test_data.r, e.test_data.g, e.test_data.b);
                switchState(PlayerState::TEST);
            } else
                ESP_LOGW(TAG, "ReadyState: ignoring event %s", getEventName(e.type));
            break;

        case PlayerState::PLAYING:
            if(e.type == EVENT_PAUSE)
                switchState(PlayerState::PAUSE);
            else if(e.type == EVENT_RESET)
                switchState(PlayerState::READY);
            else if(e.type == EVENT_RELEASE)
                switchState(PlayerState::UNLOADED);
            else
                ESP_LOGW(TAG, "PlayingState: ignoring event %s", getEventName(e.type));
            break;

        case PlayerState::PAUSE:
            if(e.type == EVENT_PLAY)
                switchState(PlayerState::PLAYING);
            else if(e.type == EVENT_RESET)
                switchState(PlayerState::READY);
            else if(e.type == EVENT_RELEASE)
                switchState(PlayerState::UNLOADED);
            else
                ESP_LOGW(TAG, "PauseState: ignoring event %s", getEventName(e.type));
            break;

        case PlayerState::TEST:
            if(e.type == EVENT_TEST) {
                testPlayback(e.test_data.r, e.test_data.g, e.test_data.b);
            } else if(e.type == EVENT_RESET)
                switchState(PlayerState::READY);
            else if(e.type == EVENT_RELEASE)
                switchState(PlayerState::UNLOADED);
            else
                ESP_LOGW(TAG, "TestState: ignoring event %s", getEventName(e.type));
            break;
    }
}

// Update

void Player::updateState() {
    if(m_state == PlayerState::PLAYING) {
#if SHOW_TRANSITION
        ESP_LOGI("state.cpp", "Update!");
#endif
        updatePlayback();
    }
}