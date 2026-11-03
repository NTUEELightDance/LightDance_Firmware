# FileDownloader Component

The `FileDownloader` component is designed to download control files (`control.dat`) and lighting effect files (`frame.dat`) from a specified server via Wi-Fi (TCP/IP) and write them to an SD card.

This component handles the coexistence logic between Wi-Fi and BLE (Bluetooth Low Energy). It automatically disables BLE functions before starting the download and re-initializes/starts BLE after the download is complete, ensuring wireless communication stability and freeing up system resources.

## Features

* **Automated Update Process**: Initiates the update task with a single function call, automatically handling BLE de-initialization, Wi-Fi connection, file downloading, Wi-Fi disconnection, and BLE restart.
* **TCP Client**: Establishes a TCP Socket connection to a specified server.
* **Resumable/Large File Writing**: Receives data in chunks using a buffer and writes to the SD card via FatFs, supporting large file transfers.
* **Identification**: Reads the Player ID from the SD card and sends it to the server before downloading to fetch the corresponding files.
* **Error Handling**: Includes mechanisms for Wi-Fi reconnection and file write error detection.

## Dependencies

This component depends on the following ESP-IDF components and internal project components:
* `fatfs`: For SD card file system operations.
* `sdmmc`: SD card driver.
* `esp_wifi`: Wi-Fi connection functionality.
* `lwip`: TCP/IP protocol stack.
* `nvs_flash`: Wi-Fi configuration storage.
* **`PT_Reader`**: For reading the Player ID (`get_sd_card_id`).
* **`Messenger`**: For controlling BLE initialization and de-initialization (`bt_receiver`).

## Configuration

The Wi-Fi SSID, password, and the TCP server's IP and Port are currently defined in the header file.
Please modify `include/tcp_client.h` to configure these settings:

```c
// include/tcp_client.h

#define TCP_WIFI_SSID      "makerspace-2.4G"   // Wi-Fi SSID
#define TCP_WIFI_PASS      "ntueemakerspace"   // Wi-Fi Password
#define TCP_SERVER_IP      "192.168.0.100"     // Server IP
#define TCP_SERVER_PORT    3333                // Server Port
```

## Usage

In your main program, ensure the SD card is mounted, then call the following API to start the update task:

```cpp
#include "tcp_client.h"

// Start the update task
// Note: This function creates a FreeRTOS task that deletes itself upon completion.
tcp_client_start_update_task();
```

## Workflow

When `tcp_client_start_update_task()` is called, the following steps are executed:

1.  **Deinit BLE**: Calls `bt_receiver_deinit()` to stop and release Bluetooth resources.
2.  **Start Wi-Fi**: Initializes Wi-Fi in Station mode and attempts to connect to the configured AP.
3.  **TCP Connect**: After a successful Wi-Fi connection, establishes a Socket connection to the TCP Server.
4.  **Handshake**: Reads the current Player ID and sends it as a string (e.g., `"1\n"`) to the server.
5.  **Download Files**:
    * Receives a 4-byte file size header (Network byte order).
    * Downloads `0:/control.dat` and writes it to the SD card.
    * Downloads `0:/frame.dat` and writes it to the SD card.
6.  **Stop Wi-Fi**: Disconnects, stops Wi-Fi, and releases resources.
7.  **Re-init BLE**: Calls `bt_receiver_init()` and `bt_receiver_start()` to restore Bluetooth receiving functionality.
8.  **Notify Main**: Sends a message to the main queue to notify that the update is complete.

## File Structure

```text
FileDownloader/
├── CMakeLists.txt          # Build configuration
├── include/
│   ├── sd_writer.h         # SD card writing function declarations
│   └── tcp_client.h        # TCP client and update task declarations (includes config)
└── src/
    ├── sd_writer.cpp       # Implementation of FatFs writing wrapper
    └── tcp_client.cpp      # Wi-Fi event handling, TCP protocol, and main update logic
```