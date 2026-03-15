#pragma once
#include <cstdint>
#include <cstdlib>

class RingBuffer {
private:
    uint8_t* buffer;
    uint64_t capacity;
    uint64_t capacity_mask;
    
    // Absolute counters
    uint64_t write_head = 0;
    uint64_t tail_head = 0;

public:
    RingBuffer(uint64_t total_size);
    ~RingBuffer();

    RingBuffer(const RingBuffer&) = delete;
    RingBuffer& operator=(const RingBuffer&) = delete;

    // HOT PATH: Branchless allocation
    inline void* allocate(uint64_t bytes) {
        uint64_t used_space = write_head - tail_head;
        uint64_t free_space = capacity - used_space;

        if (free_space < bytes) [[unlikely]] {
            return nullptr; // RAM is full, disk is too slow
        }

        // Calculate absolute memory address using the mask
        void* ptr = buffer + (write_head & capacity_mask);
        write_head += bytes;
        return ptr;
    }

    // HOT PATH: Free space when disk finishes
    inline void advance_tail(uint64_t bytes) {
        tail_head += bytes;
    }

    uint64_t get_used_space() const { return write_head - tail_head; }
    uint64_t get_free_space() const { return capacity - (write_head - tail_head); }
};