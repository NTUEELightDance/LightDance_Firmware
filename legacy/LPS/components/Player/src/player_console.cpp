#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_console.h"
#include "esp_log.h"
#include "ld_nvs.h"

#include "player.hpp"

/* ================= config ================= */

#define PROMPT_STR "cmd"

/* ================= static state (ONLY HERE) ================= */

static const char* TAG = "CONSOLE";

static esp_console_repl_t* repl = NULL;
static esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();

/* ================= command handlers ================= */

static int cmd_play(int argc, char** argv) {
    Player::getInstance().play();
    return 0;
}

static int cmd_seek(int argc, char** argv) {
    if(argc != 2) {
        printf("Usage: seek <time_us>\n");
        return 1;
    }
    uint32_t time = atoi(argv[1]);
    Player::getInstance().seek(time);
    return 0;
}

static int cmd_pause(int argc, char** argv) {
    Player::getInstance().pause();
    return 0;
}

static int cmd_stop(int argc, char** argv) {
    Player::getInstance().stop();
    return 0;
}

static int cmd_release(int argc, char** argv) {
    Player::getInstance().release();
    return 0;
}

// static int cmd_load(int argc, char** argv) {
//     Player::getInstance().load();
//     return 0;
// }

static int cmd_exit(int argc, char** argv) {
    Player::getInstance().exit();
    return 0;
}

static int cmd_test(int argc, char** argv) {
    if(argc == 1) {
        Player::getInstance().test();
        return 0;
    }

    if(argc < 4) {
        printf("Usage: test <r> <g> <b>\n");
        return 1;
    }

    int r = atoi(argv[1]);
    int g = atoi(argv[2]);
    int b = atoi(argv[3]);

    grb8_t color = grb8(r, g, b);
    ESP_LOGD(TAG, "test color conversion: stage=input r=%u g=%u b=%u", color.r, color.g, color.b);
    color = grb_gamma_u8(color, LED_WS2812B);
    ESP_LOGD(TAG, "test color conversion: stage=gamma r=%u g=%u b=%u", color.r, color.g, color.b);
    color = grb_set_brightness(color, LED_WS2812B);
    ESP_LOGD(TAG, "test color conversion: stage=brightness r=%u g=%u b=%u", color.r, color.g, color.b);

    if(r < 0) {
        r = 0;
    } else if(r > 255) {
        r = 255;
    }

    if(g < 0) {
        g = 0;
    } else if(g > 255) {
        g = 255;
    }

    if(b < 0) {
        b = 0;
    } else if(b > 255) {
        b = 255;
    }

    Player::getInstance().test(r, g, b);
    return 0;
}

static int cmd_nvs(int argc, char** argv) {
    if(argc < 2) {
        printf("Usage: nvs set <key> <value>\n");
        printf("       nvs get <key>\n");
        return 1;
    }

    if(strcmp(argv[1], "set") == 0) {
        if(argc != 4) {
            printf("Usage: nvs set <key> <value>\n");
            return 1;
        }

        errno = 0;
        char* end = NULL;
        unsigned long parsed = strtoul(argv[3], &end, 0);
        if(errno != 0 || end == argv[3] || *end != '\0' || parsed > UINT8_MAX) {
            printf("Invalid value: %s (expected 0..255)\n", argv[3]);
            return 1;
        }

        const char* key = strcmp(argv[2], "id") == 0 ? LD_NVS_KEY_PLAYER_ID : argv[2];
        esp_err_t err = ld_nvs_set_u8(key, (uint8_t)parsed);
        if(err != ESP_OK) {
            printf("NVS set failed: %s\n", esp_err_to_name(err));
            return 1;
        }

        printf("%s = %lu\n", key, parsed);
        return 0;
    }

    if(strcmp(argv[1], "get") == 0) {
        if(argc != 3) {
            printf("Usage: nvs get <key>\n");
            return 1;
        }

        const char* key = strcmp(argv[2], "id") == 0 ? LD_NVS_KEY_PLAYER_ID : argv[2];
        uint8_t value = 0;
        esp_err_t err = ld_nvs_get_u8(key, &value);
        if(err != ESP_OK) {
            printf("NVS get failed: %s\n", esp_err_to_name(err));
            return 1;
        }

        printf("%s = %u\n", key, (unsigned)value);
        return 0;
    }

    printf("Unknown NVS operation: %s\n", argv[1]);
    printf("Usage: nvs set <key> <value> | nvs get <key>\n");
    return 1;
}

/* ================= register commands ================= */

static void register_cmd(const char* name, const char* help, esp_console_cmd_func_t func) {
    esp_console_cmd_t cmd = {
        .command = name,
        .help = help,
        .hint = NULL,
        .func = func,
        .argtable = NULL,
        .func_w_context = NULL,
        .context = NULL,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

static void register_all_commands(void) {
    register_cmd("play", "start playback", &cmd_play);
    register_cmd("pause", "pause playback", &cmd_pause);
    register_cmd("stop", "stop playback", &cmd_stop);
    register_cmd("release", "release player", &cmd_release);
    // register_cmd("load", "load frames", &cmd_load);
    register_cmd("test", "test rgb output", &cmd_test);
    register_cmd("exit", "exit player", &cmd_exit);
    register_cmd("seek", "seek time", &cmd_seek);
    register_cmd("nvs", "NVS u8: nvs set <key> <value> | nvs get <key>", &cmd_nvs);
}

/* ================= console entry ================= */

void console_test(void) {
    ESP_LOGD(TAG, "UART console initialization started");

    repl_config.prompt = PROMPT_STR ">";
    repl_config.max_cmdline_length = 256;

    esp_console_register_help_command();
    register_all_commands();

    esp_console_dev_uart_config_t hw_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();

    ESP_ERROR_CHECK(esp_console_new_repl_uart(&hw_config, &repl_config, &repl));

    ESP_ERROR_CHECK(esp_console_start_repl(repl));
    ESP_LOGI(TAG, "UART console ready: prompt=%s>", PROMPT_STR);
}
