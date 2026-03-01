SD Logger Module for ESP32
===

A lightweight logging utility designed to redirect ESP_LOG messages and custom strings directly to an SD Card via the FATFS file system.

1.API Reference
---
**esp_err_t sd_logger_init(const char* log_path)**

Initializes the logger, opens the logger file, hooks into the ESP-IDF logging system.Redirects vprintf to write to the SD card.

**void sd_logger_deinit(void)**

Safely stops the logger by restores the original vprintf function, flushes remaining data, and closes the file.

**int sd_log_printf(const char* format, ...)**

Bypasses the standard ESP-IDF log level formatting to write a raw string directly to the SD card.

2.Error Message Glossary
---