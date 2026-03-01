# FileDownloader Component

The `FileDownloader` component is designed to download control files (`control.dat`) and lighting effect files (`frame.dat`) from a specified server via Wi-Fi (TCP/IP) and write them to an SD card.

This component handles the coexistence logic between Wi-Fi and BLE (Bluetooth Low Energy). It automatically disables BLE functions before starting the download, ensuring wireless communication stability and freeing up system resources for the TCP/IP stack.

## Features

* **Automated Update Process**: Initiates the update task with a single function call, handling BLE de-initialization, Wi-Fi connection, and file downloading.
* **TCP Client**: Establishes a TCP Socket connection to a specified server.
* **Resumable/Large File Writing**: Receives data in chunks using a buffer and writes to the SD card via FatFs, supporting large file transfers.
* **Identification**: Reads the Player ID from the SD card and sends it to the server before downloading to fetch the corresponding files.
* **System Reboot**: Upon successful download, it triggers a system reboot via the main command queue to ensure a clean state.

## Dependencies

This component depends on the following ESP-IDF components and internal project components:
* `fatfs`: For SD card file system operations.
* `sdmmc`: SD card driver.
* `esp_wifi`: Wi-Fi connection functionality.
* `lwip`: TCP/IP protocol stack.
* `nvs_flash`: Wi-Fi configuration storage.
* **`PT_Reader`**: For reading the Player ID (`get_sd_card_id`).
* **`Messenger`**: For controlling BLE de-initialization (`bt_receiver_deinit`) and accessing the system command queue.

## Configuration

The Wi-Fi SSID, password, and the TCP server's IP and Port are currently defined in the header file.
Please modify `include/tcp_client.h` to configure these settings:

```c
// include/tcp_client.h

#define TCP_WIFI_SSID      "lightdance"        // Wi-Fi SSID
#define TCP_WIFI_PASS      "ntueelightdance"   // Wi-Fi Password
#define TCP_SERVER_IP      "192.168.50.4"      // Server IP
#define TCP_SERVER_PORT    3333                // Server Port
```

## Usage

In your main program, ensure the SD card is mounted, then call the following API to start the update task:

```cpp
#include "tcp_client.h"

// Start the update task
// Note: This function creates a FreeRTOS task that runs in the background.
tcp_client_start_update_task();
```

## Workflow

When `tcp_client_start_update_task()` is called, the following steps are executed by the background task:

1. **Deinit BLE**: Calls `bt_receiver_deinit()` to completely stop and release Bluetooth hardware and memory resources.
2. **Start Wi-Fi**: Initializes Wi-Fi in Station mode and attempts to connect to the configured Access Point.
3. **TCP Connect**: After a successful Wi-Fi connection, establishes a Socket connection to the Python TCP Server.
4. **Handshake**: Reads the current Player ID from the SD card and sends it as a string (e.g., `"1\n"`) to the server.
5. **Download Files**:
* Receives a 4-byte file size header (Network byte order).
* Downloads `0:/control.dat` and writes it to the SD card in chunks.
* Receives the next size header, downloads `0:/frame.dat`, and writes it to the SD card.


6. **Send ACK**: Sends a `"DONE\n"` string to the server to confirm successful reception, then closes the TCP socket.
7. **Notify Main (Reboot)**: Sends an `UPLOAD_SUCCESS` message to the `sys_cmd_queue`. The main task intercepts this message, stops any current playback, and safely reboots the ESP32 to apply the updates and restore BLE functionality with a clean memory state.

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