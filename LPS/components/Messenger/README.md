# BlueTooth Receiver Component (ESP32)

This is a Bluetooth Low Energy (BLE) receiver component designed for the ESP32. It bypasses standard Bluedroid or NimBLE stacks, communicating directly with the controller via the ESP32's **HCI (Host Controller Interface)**. This achieves ultra-low latency and precise parsing of advertising packets.

Its primary function is for multi-device synchronization systems. The receiver scans for specific BLE advertising packets, parses commands and timestamps, and utilizes `esp_timer` to trigger `Player` actions at precise moments.

## ✨ Features

* **Low Latency Parsing**: Performs rapid parsing of advertising packets (`fast_parse_and_trigger`) directly within the VHCI callback function (ISR context).
* **Precise Synchronization**: Includes synchronization window logic (`sync_process_task`) to collect multiple advertising packets and calculate the average trigger time, eliminating the variance caused by wireless transmission latency.
* **Target Filtering**: Supports filtering by `Manufacturer ID` and `Target Mask` (bitmask), allowing commands to be targeted at a single device or a group of devices.
* **Command Queueing**: Manages concurrent action commands using FreeRTOS Queues and Timers.

## 📂 File Structure

```text
Messenger/
├── CMakeLists.txt          # ESP-IDF component build script
├── include/
│   └── bt_receiver.h       # External API interface and structure definitions
└── src/
    └── bt_receiver.cpp     # Core implementation (HCI commands, ISR parsing, sync logic)
```

## 🛠 Dependencies

* **ESP-IDF** (Must include `bt`, `nvs_flash`, `esp_timer`, and other standard components).
* **Player Module** (`player.hpp`): This component relies on an external `Player` singleton class to execute the actual actions (e.g., `play()`, `pause()`).

## 🚀 Usage

### 1. Configuration and Initialization

Include the header file and initialize the component in your `main` code:

```c
#include "bt_receiver.h"

void app_main(void) {
    // Define receiver configuration
    bt_receiver_config_t config = {
        .feedback_gpio_num = -1,     // Debug GPIO (Set to -1 if unused)
        .manufacturer_id = 0xABCD,   // Must match the Sender's Manufacturer ID
        .my_player_id = 0,           // This device's ID (Used for Target Mask check)
        .sync_window_us = 20000,     // Sync window size in microseconds (e.g., 20ms)
        .queue_size = 10             // Depth of the command queue
    };

    // Initialize the receiver
    ESP_ERROR_CHECK(bt_receiver_init(&config));

    // Start receiving and scanning
    ESP_ERROR_CHECK(bt_receiver_start());
}
```

### 2. Stop Receiving

If you need to stop scanning and timers:

```c
bt_receiver_stop();
```

## 📡 Protocol Definition (Manufacturer Specific Data)

The receiver parses the `AD Type = 0xFF` (Manufacturer Specific Data) section within the BLE advertising packet.
The data payload format is as follows:

| Offset | Length | Description | Notes |
| :--- | :--- | :--- | :--- |
| 0 | 2 | **Manufacturer ID** | Little Endian, must match Config |
| 2 | 1 | **CMD Info** | High 4-bit: `CMD_ID` (Identifier)<br>Low 4-bit: `CMD_TYPE` (Action Type) |
| 3 | 8 | **Target Mask** | 64-bit Mask, corresponds to `my_player_id` |
| 11 | 4 | **Delay** | Big Endian, execution delay (us) |
| 15 | 4 | **Prep Time** | Big Endian, preparation time |
| 19 | 3 | **Data** | Extra parameters (e.g., test data) |

### Supported Command Types (CMD_TYPE)

Based on the `timer_timeout_cb` implementation:

* `0x01`: Play
* `0x02`: Pause
* `0x03`: Reset
* `0x04`: Release (Resources)
* `0x05`: Load
* `0x06`: Test (Uses the `Data` field)
* `0x07`: Cancel (Cancels the schedule for a specific `CMD_ID`)

## ⚙️ Operating Principle

1.  **HCI Setup**: Upon startup, sends HCI commands to set the Event Mask (filtering LE Meta Events) and enables BLE passive scanning.
2.  **ISR Parsing**: When an advertising packet is received, `fast_parse_and_trigger` checks if it matches the Manufacturer ID and Target Mask.
3.  **Packet Queueing**: Packets meeting the criteria are pushed into `s_adv_queue`.
4.  **Synchronization Task (`sync_process_task`)**:
    * Receives packets from the Queue.
    * Collects all packets with the same `CMD_ID` within the `sync_window_us` period.
    * Calculates the average `target_execution_time` (Reception Time + Packet Delay) to reduce error.
5.  **Timed Execution**: Sets an `esp_timer` to trigger at the calculated time; the callback function calls the corresponding API of the `Player`.

## ⚠️ Notes

* **Player Dependency**: Compiling this component requires ensuring `player.hpp` is in the include path and that the linker can find the `Player` class implementation.
* **Big Endian**: The protocol uses Big Endian format for `Delay` and `Prep Time`. The sender must handle byte ordering correctly.
* **ID Uniqueness**: `CMD_ID` is used to distinguish different action batches. Reusing the same ID within a short timeframe may result in it being treated as the same processing batch.