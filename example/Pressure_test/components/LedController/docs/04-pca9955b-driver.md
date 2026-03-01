# 04 - PCA9955B Driver

Implementation: `src/pca9955b.c`.

## Responsibilities

- Initialize I2C bus/device handles
- Manage per-device shadow payload buffer
- Map public GRB input to internal RGB register order
- Flush payload through burst writes
- Recover IREF after failed transactions

## Key Structures

- `pca9955b_dev_t`
  - `i2c_dev_handle`, `i2c_addr`
  - `buffer` (`command_byte` + 15 data bytes)
  - `need_reset_IREF`

## API Semantics

- `i2c_bus_init`: create I2C master bus
- `pca9955b_init`: attach device, init IREF, clear outputs
- `set_pixel` / `write_grb` / `fill`: staged buffer updates
- `pca9955b_show`: transmit staged payload
- `pca9955b_del`: best-effort off + detach device

## Recovery Behavior

When transmit fails, `need_reset_IREF` is set.
On next `show()`, driver retries IREF reset before payload transmit.

## Constraints

- address < `0x80`
- pixel/count limits must fit configured channels per IC
