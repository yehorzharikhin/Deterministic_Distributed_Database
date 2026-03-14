#pragma once
#include <cstdint>
#include <cstdlib>
#include <new>

class ArenaAllocator {
private:
    uintptr_t current_ptr;
    uintptr_t end_ptr;
    void* original_block;

public:
    ArenaAllocator(size_t total_size) {
        original_block = std::malloc(total_size);
        if (!original_block) throw std::bad_alloc();
        
        current_ptr = reinterpret_cast<uintptr_t>(original_block);
        end_ptr = current_ptr + total_size;
    }

    ~ArenaAllocator() {
        std::free(original_block);
    }

    ArenaAllocator(const ArenaAllocator&) = delete;
    ArenaAllocator& operator=(const ArenaAllocator&) = delete;

    inline void* allocate(size_t bytes, size_t alignment) {
        uintptr_t aligned_ptr = (current_ptr + alignment - 1) & ~(alignment - 1);
        uintptr_t next_ptr = aligned_ptr + bytes;

        if (next_ptr > end_ptr) [[unlikely]] {
            return nullptr;
        }

        current_ptr = next_ptr;
        return reinterpret_cast<void*>(aligned_ptr);
    }

    inline void reset() {
        current_ptr = reinterpret_cast<uintptr_t>(original_block);
    }
};