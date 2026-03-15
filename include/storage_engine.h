#pragma once
#include "ring_buffer.h"
#include "async_disk_writer.h"
#include "wal_record.h"

class StorageEngine {
private:
    RingBuffer ram_buffer;
    AsyncDiskWriter disk_writer;
    uint64_t current_disk_offset = 0;

public:
    StorageEngine(uint64_t ram_capacity, const char* wal_filepath);

    // HOT PATH: Call this when a new network trade arrives
    inline bool insert_trade(uint64_t timestamp, int64_t price, uint32_t volume, const char* ticker) {
        WALRecord* record = static_cast<WALRecord*>(ram_buffer.allocate(sizeof(WALRecord)));
        if (!record) [[unlikely]] {
            return false; // Backpressure: System overloaded
        }

        record->timestamp = timestamp;
        record->price = price;
        record->volume = volume;
        record->ticker = pack_ticker(ticker);
        record->_padding = 0;

        disk_writer.queue_write(record, sizeof(WALRecord), current_disk_offset);
        current_disk_offset += sizeof(WALRecord);
        return true;
    }

    // HOT PATH: Call this in the main while(true) loop constantly
    inline void poll() {
        disk_writer.poll_completions([&](void* finished_ptr) {
            // We wrote exactly 1 WALRecord (32 bytes), so we free 32 bytes
            // The pointer is ignored here because disk writes complete in order
            ram_buffer.advance_tail(sizeof(WALRecord));
        });
        
        // Force flush if market is quiet
        disk_writer.flush_to_kernel(); 
    }
};