#pragma once

#include <stdio.h>
#include <string.h>
#include "esp_console.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_vfs_fat.h"

#include "player_v2.hpp"


#define PROMPT_STR "cmd"

static esp_console_repl_t* repl = NULL;
static esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();


static int sendPlay(int argc, char** argv) {
    Player::getInstance().play();
    return 0;
}

static void register_sendPlay(void) {
    const esp_console_cmd_t cmd = {.command = "play",
                                   .help = "send play",
                                   .hint = NULL,
                                   .func = &sendPlay,

                                   .argtable = NULL,
                                   .func_w_context = NULL,
                                   .context = NULL};
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

static int sendPause(int argc, char** argv) {
    Player::getInstance().pause();
    return 0;
}

static void register_sendPause(void) {
    const esp_console_cmd_t cmd = {.command = "pause",
                                   .help = "send pause",
                                   .hint = NULL,
                                   .func = &sendPause,

                                   .argtable = NULL,
                                   .func_w_context = NULL,
                                   .context = NULL};
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

static int sendTest(int argc, char** argv) {
    if (argc < 4) {
        printf("Error: Missing arguments.\n");
        printf("Usage: test <r> <g> <b>\n");
        return 1;
    }
    int raw_r = atoi(argv[1]);
    int raw_g = atoi(argv[2]);
    int raw_b = atoi(argv[3]);

    int r = (raw_r > 255) ? 255 : ((raw_r < 0) ? 0 : raw_r);
    int g = (raw_g > 255) ? 255 : ((raw_g < 0) ? 0 : raw_g);
    int b = (raw_b > 255) ? 255 : ((raw_b < 0) ? 0 : raw_b);

    if (raw_r != r || raw_g != g || raw_b != b) {
        printf("Warning: Values clamped to valid range [0-255].\n");
        printf("Corrected to: %d %d %d\n", r, g, b);
    }

    Player::getInstance().test(r, g, b);
    return 0;
}

static void register_sendTest(void) {
    const esp_console_cmd_t cmd = {.command = "test",
                                   .help = "send test",
                                   .hint = NULL,
                                   .func = &sendTest,

                                   .argtable = NULL,
                                   .func_w_context = NULL,
                                   .context = NULL};
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

static int sendReset(int argc, char** argv) {
    Player::getInstance().reset();
    return 0;
}

static void register_sendReset(void) {
    const esp_console_cmd_t cmd = {.command = "reset",
                                   .help = "send reset",
                                   .hint = NULL,
                                   .func = &sendReset,

                                   .argtable = NULL,
                                   .func_w_context = NULL,
                                   .context = NULL};
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

static int sendRelease(int argc, char** argv) {
    Player::getInstance().release();
    return 0;
}

static void register_sendRelease(void) {
    const esp_console_cmd_t cmd = {.command = "release",
                                   .help = "send release",
                                   .hint = NULL,
                                   .func = &sendRelease,

                                   .argtable = NULL,
                                   .func_w_context = NULL,
                                   .context = NULL};
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

static int sendLoad(int argc, char** argv) {
    Player::getInstance().load();
    return 0;
}
static void register_sendLoad(void) {
    const esp_console_cmd_t cmd = {.command = "load",
                                   .help = "send load",
                                   .hint = NULL,
                                   .func = &sendLoad,

                                   .argtable = NULL,
                                   .func_w_context = NULL,
                                   .context = NULL};
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

static int SendExit(int argc, char** argv) {
    Player::getInstance().exit();
    return 0;
}
static void register_sendExit(void) {
    const esp_console_cmd_t cmd = {.command = "exit",
                                   .help = "send exit",
                                   .hint = NULL,
                                   .func = &SendExit,

                                   .argtable = NULL,
                                   .func_w_context = NULL,
                                   .context = NULL};
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

static int stop_console(int argc, char** argv) {
    esp_console_repl_t* repl = (esp_console_repl_t*)argv;
    if (repl != NULL) {
        //esp_console_del_repl(repl);
    }
    return 0;
}

void esp_console_del_repl(esp_console_repl_t* repl);

static void register_stop_console(void) {
    const esp_console_cmd_t cmd = {.command = "stop",
                                   .help = "stop console",
                                   .hint = NULL,
                                   .func = &stop_console,

                                   .argtable = NULL,
                                   .func_w_context = NULL,
                                   .context = NULL};
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

static void register_cmd() {
    register_sendPlay();
    register_sendPause();
    register_sendReset();
    register_sendRelease();
    register_sendLoad();
    register_sendTest();
    register_sendExit();
    register_stop_console();
}

void start_console() {

    /* Prompt to be printed before each line.
     * This can be customized, made dynamic, etc.
     */
    repl_config.prompt = PROMPT_STR ">";
    repl_config.max_cmdline_length = 1024;

    esp_console_register_help_command();
    register_cmd();

    esp_console_dev_uart_config_t hw_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&hw_config, &repl_config, &repl));

    ESP_ERROR_CHECK(esp_console_start_repl(repl));
}