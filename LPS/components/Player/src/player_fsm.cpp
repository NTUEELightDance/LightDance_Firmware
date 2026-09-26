#include "esp_err.h"
#include "esp_log.h"
#include "player.hpp"

static const char* TAG = "PLAYER_FSM";

const char* getEventName(int type) {
    switch(type) {
        case EVENT_PLAY:
            return "PLAY";
        case EVENT_PAUSE:
            return "PAUSE";
        case EVENT_STOP:
            return "STOP";
        case EVENT_RELEASE:
            return "RELEASE";
        case EVENT_LOAD:
            return "LOAD";
        case EVENT_TEST:
            return "TEST";
        case EVENT_EXIT:
            return "EXIT";
        case EVENT_SEEK:
            return "SEEK";
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

    ESP_LOGI(TAG, "state transition: %s -> %s", getStateName(m_state), getStateName(newState));

    // Exit
    switch(m_state) {
        default:
#if SHOW_TRANSITION
            ESP_LOGD(TAG, "leaving state: %s", getStateName(m_state));
#endif
            break;
    }

    m_state = newState;

    // Enter
    switch(m_state) {
        case PlayerState::UNLOADED: {
#if SHOW_TRANSITION
            ESP_LOGD(TAG, "entering state: UNLOADED");
#endif
            if(releaseResources() == ESP_OK) {
                ESP_LOGD(TAG, "player resources released");
                vTaskDelay(pdMS_TO_TICKS(LD_CFG_PLAYER_BOOT_RELOAD_DELAY_MS));
                Event e;
                e.type = EVENT_LOAD;
                sendEvent(e);
            } else {
                ESP_LOGW(TAG, "player resource release failed; retrying in %u ms", (unsigned)LD_CFG_PLAYER_RETRY_DELAY_MS);
                vTaskDelay(pdMS_TO_TICKS(LD_CFG_PLAYER_RETRY_DELAY_MS));
                Event e;
                e.type = EVENT_RELEASE;
                sendEvent(e);
            }
        } break;

        case PlayerState::READY: {
#if SHOW_TRANSITION
            ESP_LOGD(TAG, "entering state: READY");
#endif
            if(resetPlayback() == ESP_OK) {
                ESP_LOGD(TAG, "playback resources reset and ready");
            } else {
                ESP_LOGE(TAG, "playback reset failed; returning to UNLOADED state");
                switchState(PlayerState::UNLOADED);
            }
        } break;

        case PlayerState::PLAYING: {
#if SHOW_TRANSITION
            ESP_LOGD(TAG, "entering state: PLAYING");
#endif
            startPlayback();
        } break;

        case PlayerState::PAUSE: {
#if SHOW_TRANSITION
            ESP_LOGD(TAG, "entering state: PAUSED");
#endif
            pausePlayback();
        } break;

        case PlayerState::TEST: {
#if SHOW_TRANSITION
            ESP_LOGD(TAG, "entering state: TEST");
#endif
            resetPlayback();
            testPlayback(m_test_data);
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
                else {
                    ESP_LOGW(TAG, "player resource acquisition failed; retrying in %u ms", (unsigned)LD_CFG_PLAYER_RETRY_DELAY_MS);
                    vTaskDelay(pdMS_TO_TICKS(LD_CFG_PLAYER_RETRY_DELAY_MS));
                    Event e;
                    e.type = EVENT_LOAD;
                    sendEvent(e);
                }
            } else if(e.type == EVENT_RELEASE) {
                switchState(PlayerState::UNLOADED);
            } else
                ESP_LOGW(TAG, "event ignored: state=UNLOADED event=%s", getEventName(e.type));
            break;

        case PlayerState::READY:
            if(e.type == EVENT_PLAY)
                switchState(PlayerState::PLAYING);
            else if(e.type == EVENT_RELEASE)
                switchState(PlayerState::UNLOADED);
            else if(e.type == EVENT_TEST) {
                m_test_data = e.test_data;
                switchState(PlayerState::TEST);
            } else if(e.type == EVENT_SEEK) {
                esp_err_t err = Player::getInstance().set_time_us(e.data);
                if(err != ESP_OK) {
                    ESP_LOGE(TAG, "seek failed: target_us=%lu err=%s", (unsigned long)e.data, esp_err_to_name(err));
                } else {
                    ESP_LOGI(TAG, "seek request processed: target_us=%lu", (unsigned long)e.data);
                }
            } else
                ESP_LOGW(TAG, "event ignored: state=READY event=%s", getEventName(e.type));
            break;

        case PlayerState::PLAYING:
            if(e.type == EVENT_PAUSE)
                switchState(PlayerState::PAUSE);
            else if(e.type == EVENT_STOP)
                switchState(PlayerState::READY);
            else if(e.type == EVENT_RELEASE)
                switchState(PlayerState::UNLOADED);
            else
                ESP_LOGW(TAG, "event ignored: state=PLAYING event=%s", getEventName(e.type));
            break;

        case PlayerState::PAUSE:
            if(e.type == EVENT_PLAY)
                switchState(PlayerState::PLAYING);
            else if(e.type == EVENT_STOP)
                switchState(PlayerState::READY);
            else if(e.type == EVENT_RELEASE)
                switchState(PlayerState::UNLOADED);
            else
                ESP_LOGW(TAG, "event ignored: state=PAUSED event=%s", getEventName(e.type));
            break;

        case PlayerState::TEST:
            if(e.type == EVENT_TEST) {
                m_test_data = e.test_data;
                switchState(PlayerState::TEST);
            } else if(e.type == EVENT_STOP)
                switchState(PlayerState::READY);
            else if(e.type == EVENT_RELEASE) {
                switchState(PlayerState::UNLOADED);
            } else
                ESP_LOGW(TAG, "event ignored: state=TEST event=%s", getEventName(e.type));
            break;
    }
}

// Update

void Player::updateState() {
    if(m_state == PlayerState::PLAYING) {
#if SHOW_TRANSITION
        ESP_LOGV(TAG, "updating state: PLAYING");
#endif
        updatePlayback();
    }
    if(m_state == PlayerState::TEST) {
#if SHOW_TRANSITION
        ESP_LOGV(TAG, "updating state: TEST");
#endif
        updatePlayback();
    }
}
