#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "console.h"


extern "C" void app_main(void)
{
    Player::getInstance().init();
    start_console();
}