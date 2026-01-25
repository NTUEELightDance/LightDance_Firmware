#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_console.h"
#include "esp_log.h"
#include "esp_system.h"

#include "player.hpp"

/* ================= config ================= */

#define PROMPT_STR "cmd"

/* ================= static state (ONLY HERE) ================= */

static const char* TAG = "console";

static esp_console_repl_t* repl = NULL;
static esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();

/* ================= command handlers ================= */

static int cmd_play(int argc, char** argv) {
    Player::getInstance().play();
    return 0;
}

static int cmd_pause(int argc, char** argv) {
    Player::getInstance().pause();
    return 0;
}

static int cmd_reset(int argc, char** argv) {
    Player::getInstance().reset();
    return 0;
}

static int cmd_release(int argc, char** argv) {
    Player::getInstance().release();
    return 0;
}

static int cmd_load(int argc, char** argv) {
    Player::getInstance().load();
    return 0;
}

static int cmd_exit(int argc, char** argv) {
    Player::getInstance().exit();
    return 0;
}

static int cmd_test(int argc, char** argv) {
    if(argc < 4) {
        printf("Usage: test <r> <g> <b>\n");
        return 1;
    }

    int r = atoi(argv[1]);
    int g = atoi(argv[2]);
    int b = atoi(argv[3]);

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
    register_cmd("reset", "reset playback", &cmd_reset);
    register_cmd("release", "release player", &cmd_release);
    register_cmd("load", "load frames", &cmd_load);
    register_cmd("test", "test rgb output", &cmd_test);
    register_cmd("exit", "exit player", &cmd_exit);
}

/* ================= console entry ================= */

void console_test(void) {
    ESP_LOGI(TAG, "starting console");

    repl_config.prompt = PROMPT_STR ">";
    repl_config.max_cmdline_length = 1024;

    esp_console_register_help_command();
    register_all_commands();

    esp_console_dev_uart_config_t hw_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();

    ESP_ERROR_CHECK(esp_console_new_repl_uart(&hw_config, &repl_config, &repl));

    ESP_ERROR_CHECK(esp_console_start_repl(repl));
}
