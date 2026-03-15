#include "../include/storage_engine.h"

StorageEngine::StorageEngine(uint64_t ram_capacity, const char* wal_filepath)
    : ram_buffer(ram_capacity), disk_writer(wal_filepath) {
}