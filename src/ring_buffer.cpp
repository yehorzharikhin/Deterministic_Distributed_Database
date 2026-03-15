#include "../include/ring_buffer.h"
#include <new>
#include <stdexcept>

RingBuffer::RingBuffer(uint64_t total_size) {
    // Assert capacity is a perfect power of 2 for bitwise masking
    if ((total_size & (total_size - 1)) != 0) [[unlikely]] {
        throw std::invalid_argument("Capacity must be a power of 2");
    }
    
    capacity = total_size;
    capacity_mask = capacity - 1;
    buffer = static_cast<uint8_t*>(std::malloc(capacity));
    
    if (!buffer) throw std::bad_alloc();
}

RingBuffer::~RingBuffer() {
    std::free(buffer);
}