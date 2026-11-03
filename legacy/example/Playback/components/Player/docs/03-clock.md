# 03 - Player Clock & Metronome

This module provides the precise timing infrastructure required for smooth animation playback. It is divided into two distinct classes: the **Metronome** (which generates periodic events) and the **Clock** (which tracks playback time).

## 1. Architecture

### 1.1 PlayerMetronome
The Metronome uses the ESP32's hardware **GPTimer** (General Purpose Timer) to generate high-precision periodic interrupts.
* **Role:** Wakes up the main player task at a fixed interval (e.g., every 10ms for 100 FPS).
* **Mechanism:**
    * Configures a hardware timer with 1 MHz resolution.
    * Sets an auto-reload alarm.
    * In the ISR (Interrupt Service Routine), it sends a direct task notification to the Player Task.

### 1.2 PlayerClock
The Clock manages the logical timeline of the animation. It abstracts "system time" into "playback time," handling states like Pause, Resume, and Reset.
* **Role:** Provides the `now_us()` timestamp used by the FrameBuffer to interpolate frames.
* **Mechanism:**
    * Stores an `accumulated_us` offset.
    * When **Running**: Returns `accumulated_us + (system_now - start_time)`.
    * When **Paused**: Returns `accumulated_us` (time stops advancing).
    * Can optionally control an embedded `PlayerMetronome`.

---

## 2. Metronome Implementation Details

The `PlayerMetronome` is designed for low-latency task notification. Instead of using a heavy semaphore or queue, it uses FreeRTOS **Task Notifications**.

```cpp
// ISR Callback
static bool IRAM_ATTR timer_on_alarm_cb(...) {
    // Directly notify the registered task
    xTaskNotifyFromISR(task, NOTIFICATION_UPDATE, eSetBits, &woken);
    return woken == pdTRUE; // Request context switch if needed
}
```

This ensures that as soon as the hardware timer fires, the Player Task is unblocked immediately, minimizing jitter in frame rendering.

---

## 3. Clock Implementation Details

The `PlayerClock` ensures that the animation timeline is consistent, even if the system has been running for days.

### State Machine

| State | `now_us()` Behavior | Metronome State |
| :--- | :--- | :--- |
| **STOPPED** | Returns 0 (or reset value). | Stopped |
| **RUNNING** | Returns valid increasing time. | Running (firing interrupts) |
| **PAUSED** | Returns the time at the moment of pause. | Stopped (no interrupts) |

### Usage Logic

```cpp
// 1. Initialization
PlayerClock clock;
clock.init(true, my_task_handle, 10000); // With metronome, 10ms period

// 2. Playback Control
clock.start(); // Starts timer, sets reference timestamp
// ... animation playing ...
clock.pause(); // Stops timer, saves accumulated time
// ... system idle ...
clock.start(); // Resumes timer, adds new delta to accumulated time

// 3. Rendering Loop
int64_t frame_time = clock.now_us();
frameBuffer.compute(frame_time / 1000); // Convert to ms
```

---

## 4. API Reference

### PlayerMetronome
* **`init(task, period_us)`**: Sets up the GPTimer to notify `task` every `period_us`.
* **`start() / stop()`**: Enables/Disables the hardware timer.
* **`set_period_us(us)`**: Dynamically changes the tick rate (e.g., for variable speed playback).

### PlayerClock
* **`init(with_metronome, ...)`**: Initializes the clock. If `with_metronome` is true, it also initializes the internal metronome.
* **`start()`**: Begins/Resumes time tracking and starts the metronome.
* **`pause()`**: Freezes time tracking and stops the metronome.
* **`reset()`**: Sets time back to 0.
* **`now_us()`**: Returns the current playback time in microseconds.

---
