# Player API Guide 

This document is written for the player team. It explains what the frame system provides, how to use it correctly, and what assumptions the system makes. 



## 1. API


```c
bool read_frame(table_frame_t *playerbuffer);
```

This function delivers the **next animation frame** for playback.

Internally:

A background task continuously prepares frames
Synchronization and data integrity are fully handled by the system

From the player’s point of view, this behaves like a blocking frame queue.

---

## 2. Initialization

### Initialization (once, at startup)

Player code **does not open files directly**.
Initialization is handled at the system level:

```c
frame_system_init("/sdcard/frame.bin");
```

This must be called **before** any call to `read_frame()`.


### Key points

* Call `read_frame()` **once per playback tick**
* Each call advances the stream by **exactly one frame**
* Do not call it multiple times per frame

---

## 3. What Happens When read_frame() Is Called

When the player calls `read_frame()`:

1. The function waits until the next frame has been fully prepared
2. The frame is copied into the caller-provided buffer
3. The internal buffer is released so the next frame can be loaded

### Guarantees

* The returned frame is complete and valid
* Frame order is preserved
* No partial or corrupted data is ever returned

### Blocking behavior

* `read_frame()` may block briefly if the next frame is not ready yet
* Under normal operation, this delay is well below one frame interval

This blocking is intentional and required to maintain correctness.

---


