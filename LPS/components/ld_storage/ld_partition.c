#include "ld_partition.h"

#include "esp_check.h"
#include "esp_log.h"
#include "esp_partition.h"

#define LIGHT_TABLE_PARTITION_LABEL "light_table"
#define LD_PARTITION_ERASE_SIZE 4096

static const char* TAG = "ld_partition";

static size_t align_up(size_t value, size_t alignment) {
    return (value + alignment - 1) / alignment * alignment;
}

static esp_err_t find_light_table_partition(const esp_partition_t** partition) {
    ESP_RETURN_ON_FALSE(partition != NULL, ESP_ERR_INVALID_ARG, TAG, "partition pointer is NULL");

    *partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, LIGHT_TABLE_PARTITION_LABEL);
    ESP_RETURN_ON_FALSE(*partition != NULL, ESP_ERR_NOT_FOUND, TAG, "partition not found: %s", LIGHT_TABLE_PARTITION_LABEL);

    return ESP_OK;
}

static esp_err_t check_partition_range(const esp_partition_t* partition, size_t offset, size_t size) {
    ESP_RETURN_ON_FALSE(partition != NULL, ESP_ERR_INVALID_ARG, TAG, "partition is NULL");
    ESP_RETURN_ON_FALSE(offset <= partition->size && size <= partition->size - offset,
                        ESP_ERR_INVALID_SIZE,
                        TAG,
                        "partition range out of bounds: offset=%u, size=%u, partition_size=%u",
                        (unsigned)offset,
                        (unsigned)size,
                        (unsigned)partition->size);

    return ESP_OK;
}

esp_err_t ld_partition_writer_begin(ld_partition_writer_t* writer, size_t total_size) {
    return ld_partition_writer_begin_at(writer, 0, total_size);
}

esp_err_t ld_partition_writer_begin_at(ld_partition_writer_t* writer, size_t offset, size_t total_size) {
    ESP_RETURN_ON_FALSE(writer != NULL, ESP_ERR_INVALID_ARG, TAG, "writer is NULL");

    writer->partition = NULL;
    writer->offset = 0;
    writer->end_offset = 0;

    const esp_partition_t* partition = NULL;
    ESP_RETURN_ON_ERROR(find_light_table_partition(&partition), TAG, "failed to find partition");
    ESP_RETURN_ON_ERROR(check_partition_range(partition, offset, total_size), TAG, "invalid write size");
    ESP_RETURN_ON_FALSE(offset % LD_PARTITION_ERASE_SIZE == 0, ESP_ERR_INVALID_ARG, TAG, "erase offset must be %u-byte aligned: offset=%u", (unsigned)LD_PARTITION_ERASE_SIZE, (unsigned)offset);

    size_t erase_size = align_up(total_size, LD_PARTITION_ERASE_SIZE);
    ESP_RETURN_ON_ERROR(check_partition_range(partition, offset, erase_size), TAG, "invalid erase size");

    if(erase_size > 0) {
        ESP_RETURN_ON_ERROR(esp_partition_erase_range(partition, offset, erase_size), TAG, "failed to erase partition");
    }

    writer->partition = partition;
    writer->offset = offset;
    writer->end_offset = offset + total_size;

    return ESP_OK;
}

esp_err_t ld_partition_writer_write(ld_partition_writer_t* writer, const void* data, size_t size) {
    ESP_RETURN_ON_FALSE(writer != NULL, ESP_ERR_INVALID_ARG, TAG, "writer is NULL");
    ESP_RETURN_ON_FALSE(size == 0 || data != NULL, ESP_ERR_INVALID_ARG, TAG, "data is NULL");
    ESP_RETURN_ON_FALSE(writer->partition != NULL, ESP_ERR_INVALID_STATE, TAG, "writer is not started");
    ESP_RETURN_ON_FALSE(writer->offset <= writer->end_offset && size <= writer->end_offset - writer->offset,
                        ESP_ERR_INVALID_SIZE,
                        TAG,
                        "write exceeds end offset: offset=%u, size=%u, end_offset=%u",
                        (unsigned)writer->offset,
                        (unsigned)size,
                        (unsigned)writer->end_offset);

    if(size > 0) {
        ESP_RETURN_ON_ERROR(esp_partition_write(writer->partition, writer->offset, data, size), TAG, "failed to write partition");
    }

    writer->offset += size;

    return ESP_OK;
}

esp_err_t ld_partition_writer_end(ld_partition_writer_t* writer) {
    ESP_RETURN_ON_FALSE(writer != NULL, ESP_ERR_INVALID_ARG, TAG, "writer is NULL");
    ESP_RETURN_ON_FALSE(writer->partition != NULL, ESP_ERR_INVALID_STATE, TAG, "writer is not started");
    ESP_RETURN_ON_FALSE(writer->offset == writer->end_offset, ESP_ERR_INVALID_SIZE, TAG, "write is incomplete: offset=%u, end_offset=%u", (unsigned)writer->offset, (unsigned)writer->end_offset);

    writer->partition = NULL;
    writer->offset = 0;
    writer->end_offset = 0;

    return ESP_OK;
}

esp_err_t ld_partition_reader_begin(ld_partition_reader_t* reader, size_t total_size) {
    return ld_partition_reader_begin_at(reader, 0, total_size);
}

esp_err_t ld_partition_reader_begin_at(ld_partition_reader_t* reader, size_t offset, size_t total_size) {
    ESP_RETURN_ON_FALSE(reader != NULL, ESP_ERR_INVALID_ARG, TAG, "reader is NULL");

    reader->partition = NULL;
    reader->offset = 0;
    reader->end_offset = 0;

    const esp_partition_t* partition = NULL;
    ESP_RETURN_ON_ERROR(find_light_table_partition(&partition), TAG, "failed to find partition");
    ESP_RETURN_ON_ERROR(check_partition_range(partition, offset, total_size), TAG, "invalid read size");

    reader->partition = partition;
    reader->offset = offset;
    reader->end_offset = offset + total_size;

    return ESP_OK;
}

esp_err_t ld_partition_reader_read(ld_partition_reader_t* reader, void* dst, size_t dst_size, size_t* read_size) {
    ESP_RETURN_ON_FALSE(reader != NULL, ESP_ERR_INVALID_ARG, TAG, "reader is NULL");
    ESP_RETURN_ON_FALSE(read_size != NULL, ESP_ERR_INVALID_ARG, TAG, "read_size is NULL");
    ESP_RETURN_ON_FALSE(dst != NULL, ESP_ERR_INVALID_ARG, TAG, "dst is NULL");
    ESP_RETURN_ON_FALSE(dst_size > 0, ESP_ERR_INVALID_SIZE, TAG, "dst_size is zero");
    ESP_RETURN_ON_FALSE(reader->partition != NULL, ESP_ERR_INVALID_STATE, TAG, "reader is not started");
    ESP_RETURN_ON_FALSE(
        reader->offset <= reader->end_offset, ESP_ERR_INVALID_SIZE, TAG, "reader offset exceeds end offset: offset=%u, end_offset=%u", (unsigned)reader->offset, (unsigned)reader->end_offset);

    *read_size = 0;

    size_t remain_size = reader->end_offset - reader->offset;
    if(remain_size == 0) {
        return ESP_OK;
    }

    size_t size = dst_size < remain_size ? dst_size : remain_size;

    if(size > 0) {
        ESP_RETURN_ON_ERROR(esp_partition_read(reader->partition, reader->offset, dst, size), TAG, "failed to read partition");
    }

    reader->offset += size;
    *read_size = size;

    return ESP_OK;
}

esp_err_t ld_partition_reader_end(ld_partition_reader_t* reader) {
    ESP_RETURN_ON_FALSE(reader != NULL, ESP_ERR_INVALID_ARG, TAG, "reader is NULL");
    ESP_RETURN_ON_FALSE(reader->partition != NULL, ESP_ERR_INVALID_STATE, TAG, "reader is not started");
    ESP_RETURN_ON_FALSE(reader->offset == reader->end_offset, ESP_ERR_INVALID_SIZE, TAG, "read is incomplete: offset=%u, end_offset=%u", (unsigned)reader->offset, (unsigned)reader->end_offset);

    reader->partition = NULL;
    reader->offset = 0;
    reader->end_offset = 0;

    return ESP_OK;
}
