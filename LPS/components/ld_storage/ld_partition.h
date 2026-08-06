#pragma once

#include <stddef.h>

#include "esp_err.h"
#include "esp_partition.h"

typedef enum {
    LD_LT_FILE_CONTROL = 1,
    LD_LT_FILE_FRAME = 2,
} ld_storage_file_t;

typedef struct {
    const esp_partition_t* partition;
    size_t offset;
    size_t end_offset;
} ld_partition_writer_t;

typedef struct {
    const esp_partition_t* partition;
    size_t offset;
    size_t end_offset;
} ld_partition_reader_t;

esp_err_t ld_partition_writer_begin(ld_partition_writer_t* writer, size_t total_size);
esp_err_t ld_partition_writer_begin_at(ld_partition_writer_t* writer, size_t offset, size_t total_size);
esp_err_t ld_partition_writer_write(ld_partition_writer_t* writer, const void* data, size_t size);
esp_err_t ld_partition_writer_end(ld_partition_writer_t* writer);

esp_err_t ld_partition_reader_begin(ld_partition_reader_t* reader, size_t total_size);
esp_err_t ld_partition_reader_begin_at(ld_partition_reader_t* reader, size_t offset, size_t total_size);
esp_err_t ld_partition_reader_read(ld_partition_reader_t* reader, void* dst, size_t dst_size, size_t* read_size);
esp_err_t ld_partition_reader_end(ld_partition_reader_t* reader);

esp_err_t ld_file_writer_begin();
esp_err_t ld_file_writer_write();
esp_err_t ld_file_writer_end();