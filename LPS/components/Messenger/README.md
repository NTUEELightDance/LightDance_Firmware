# BlueTooth Receiver Component (ESP32)

This is a Bluetooth Low Energy (BLE) receiver component designed for the ESP32. It bypasses standard Bluedroid or NimBLE stacks, communicating directly with the controller via the ESP32's **HCI (Host Controller Interface)**. This achieves ultra-low latency, precise parsing of advertising packets, and includes a **Status Reporting mechanism**.

Its primary function is for multi-device synchronization systems. The receiver scans for specific BLE advertising packets, parses commands, synchronizes triggers using a windowing algorithm, and provides real-time status feedback upon request.

## ✨ Features

* **Low Latency Parsing**: Performs rapid parsing of advertising packets (`fast_parse_and_trigger`) directly within the VHCI callback function (ISR context).
* **Precise Synchronization**: Includes synchronization window logic (`sync_process_task`) to collect multiple advertising packets and calculate the average trigger time, eliminating the variance caused by wireless transmission latency.
* **Status Feedback**: Responds to CHECK commands by reporting the current state and the dynamic remaining time until the next action.
* **Target Filtering**: Supports filtering by `Manufacturer ID` and `Target Mask` (bitmask), allowing commands to be targeted at a single device or a group of devices.
* **Command Queueing**: Manages concurrent action commands using FreeRTOS Queues and Timers.

## 📂 File Structure

```text
Messenger/
├── CMakeLists.txt          # ESP-IDF component build script
├── include/
│   └── bt_receiver.h       # External API interface and structure definitions
└── src/
    └── bt_receiver.cpp     # Core implementation (HCI commands, ISR parsing, sync logic, ACK task)

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
        .manufacturer_id = 0xFFFF,   // Must match the Sender's Manufacturer ID
        .my_player_id = 0,           // This device's ID (Used for Target Mask check)
        .sync_window_us = 500000,    // Sync window size (e.g., 500ms)
        .queue_size = 20             // Depth of the command queue
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

## 📡 Protocol Definition

### 1. Received Packet (From Sender)

The receiver parses the `AD Type = 0xFF` (Manufacturer Specific Data) section within the BLE advertising packet.
The data payload format is as follows:

| Offset | Length | Description | Notes |
| :--- | :--- | :--- | :--- |
| 0 | 3 | **Manufacturer ID** | Little Endian, must match Config |
| 3 | 1 | **CMD Info** | High 4-bit: `CMD_ID` (Identifier)<br>Low 4-bit: `CMD_TYPE` (Action Type) |
| 4 | 8 | **Target Mask** | 64-bit Mask, corresponds to `my_player_id` |
| 12 | 4 | **Delay** | Big Endian, execution delay (us) |
| 16 | 4 | **Prep Time** | Big Endian, preparation time |
| 20 | 3 | **Data** | Extra parameters (e.g., test data) |

### 2. Transmitted ACK Packet (To Sender)

When a `CHECK` command is received, the receiver broadcasts an ACK packet.

| Offset | Length | Description | Notes |
| --- | --- | --- | --- |
| 0 | 2 | **Manufacturer ID** | `0xFFFF` |
| 2 | 1 | **Packet Type** | `0x08` (CMD_TYPE_ACK) |
| 3 | 1 | **My ID** | The `my_player_id` of this device |
| 4 | 1 | **CMD ID** | ID of the command being acknowledged |
| 5 | 1 | **CMD Type** | Action type (e.g., PLAY, PAUSE) |
| 6 | 4 | **Delay** | Big Endian, the locked delay value |
| 10 | 1 | **State** | Current Player State (e.g., READY, PLAYING) |

### Supported Command Types (CMD_TYPE)

Based on the `timer_timeout_cb` implementation:

* `0x01`: Play
* `0x02`: Pause
* `0x03`: Reset
* `0x04`: Release (Resources)
* `0x05`: Load
* `0x06`: Test (Uses the `Data` field)
* `0x07`: Cancel (Cancels the schedule for a specific `CMD_ID`)
* `0x08`: Check (Triggers immediate status report)

## ⚙️ Operating Principle

1. **HCI Setup**: Configures Event Mask and enables passive scanning.
2. **Packet Processing**:
* Valid packets are pushed to `sync_process_task`.
* **Sync Logic**: Averages timestamps within `sync_window_us` to reduce jitter.


3. **Command Locking & Timestamping**:
* When a command is locked, `s_last_locked_cmd` records the command info and the **lock timestamp** (`esp_timer_get_time()`).

4. **CHECK Command Handling**:
    1. Retrieves current state from `Player::getInstance().getState()`.
    2. Calculates **Remaining Time**: `(Lock Timestamp + Original Delay) - Current Time`.
    3. Broadcasts ACK containing the remaining time and state.