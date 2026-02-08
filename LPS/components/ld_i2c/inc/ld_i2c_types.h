#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "driver/gpio.h"
#include "driver/i2c_master.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ld_i2c_bus_t ld_i2c_bus_t;
typedef ld_i2c_bus_t* ld_i2c_bus_handle_t;

typedef struct ld_i2c_dev_t ld_i2c_dev_t;
typedef ld_i2c_dev_t* ld_i2c_dev_handle_t;

#ifdef __cplusplus
}
#endif