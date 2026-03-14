#include <iostream>
#include <chrono>
#include <vector>
#include <cassert>
#include "../include/arena_allocator.hpp"

// A global volatile pointer to trick the compiler into NOT deleting our benchmark loop
volatile void* volatile_sink;

void test_correctness() {
    std::cout << "--- Running Correctness Tests ---\n";
    ArenaAllocator arena(1024); // Tiny 1KB arena

    // 1. Check basic alignment (request 13 bytes, align to 8)
    void* ptr1 = arena.allocate(13, 8);
    assert(reinterpret_cast<uintptr_t>(ptr1) % 8 == 0 && "Pointer 1 is not 8-byte aligned!");

    // 2. Check strict SIMD alignment (request 5 bytes, align to 64)
    void* ptr2 = arena.allocate(5, 64);
    assert(reinterpret_cast<uintptr_t>(ptr2) % 64 == 0 && "Pointer 2 is not 64-byte aligned!");

    // 3. Check Out-Of-Memory bounds (request 2KB, should fail gracefully)
    void* ptr3 = arena.allocate(2048, 8);
    assert(ptr3 == nullptr && "Arena did not return nullptr on OOM!");

    std::cout << "[PASS] All memory alignments and bounds checks are mathematically correct.\n\n";
}

void test_performance() {
    std::cout << "--- Running Performance Benchmark ---\n";
    const int ITERATIONS = 10'000'000;
    const size_t ALLOC_SIZE = 64; // Typical CPU cache line size
    const size_t ALIGNMENT = 64;

    // --- OS MALLOC BENCHMARK ---
    std::vector<void*> malloc_ptrs;
    malloc_ptrs.reserve(ITERATIONS);

    auto start_malloc = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < ITERATIONS; ++i) {
        void* ptr = std::malloc(ALLOC_SIZE);
        volatile_sink = ptr; // Prevent Dead Code Elimination
        malloc_ptrs.push_back(ptr);
    }
    auto end_malloc = std::chrono::high_resolution_clock::now();

    for (void* ptr : malloc_ptrs) {
        std::free(ptr);
    }

    // --- CUSTOM ARENA BENCHMARK ---
    // 10M allocations of 64 bytes is ~640 MB. We allocate a 1 GB Arena.
    ArenaAllocator arena(1024ULL * 1024ULL * 1024ULL);

    auto start_arena = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < ITERATIONS; ++i) {
        void* ptr = arena.allocate(ALLOC_SIZE, ALIGNMENT);
        volatile_sink = ptr; // Prevent Dead Code Elimination
    }
    auto end_arena = std::chrono::high_resolution_clock::now();

    // --- RESULTS ---
    std::chrono::duration<double, std::nano> time_malloc = end_malloc - start_malloc;
    std::chrono::duration<double, std::nano> time_arena = end_arena - start_arena;

    double malloc_ns_per_alloc = time_malloc.count() / ITERATIONS;
    double arena_ns_per_alloc = time_arena.count() / ITERATIONS;

    std::cout << "std::malloc     : " << malloc_ns_per_alloc << " nanoseconds / allocation\n";
    std::cout << "ArenaAllocator  : " << arena_ns_per_alloc << " nanoseconds / allocation\n";
    std::cout << "Speedup         : " << (malloc_ns_per_alloc / arena_ns_per_alloc) << "x faster\n";
}

int main() {
    test_correctness();
    test_performance();
    return 0;
}