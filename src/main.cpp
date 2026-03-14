#include <iostream>
#include <chrono>
#include <vector>
#include <cassert>
#include "../include/arena_allocator.hpp"
#include "../include/wal_record.hpp"

// A global volatile pointer to trick the compiler into NOT deleting our benchmark loop
volatile void* volatile_sink;

void test_arena_correctness() {
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

void test_arena_performance() {
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
    std::cout << "Speedup         : " << (malloc_ns_per_alloc / arena_ns_per_alloc) << "x faster\n\n";
}

void test_wal_record() {
    std::cout << "--- Running WALRecord Layout & Logic Tests ---\n";

    // 1. Verify Compiler is not injecting hidden padding
    assert(sizeof(WALRecord) == 32 && "WALRecord is not exactly 32 bytes!");
    assert(alignof(WALRecord) == 8 && "WALRecord alignment is not exactly 8 bytes!");

    // 2. Test Ticker Packing (Standard ticker)
    uint64_t aapl_packed = pack_ticker("AAPL");
    assert(unpack_ticker(aapl_packed) == "AAPL" && "Failed to pack/unpack AAPL");

    // 3. Test Ticker Packing (Max length ticker)
    uint64_t brka_packed = pack_ticker("BRK.A");
    assert(unpack_ticker(brka_packed) == "BRK.A" && "Failed to pack/unpack BRK.A");

    // 4. Test Ticker Packing (Truncation safety - prevent buffer overflows)
    uint64_t long_packed = pack_ticker("TOOLONGTICKER");
    assert(unpack_ticker(long_packed) == "TOOLONGT" && "Failed to safely truncate long ticker");

    // 5. Fast single-cycle integer comparison test
    assert(aapl_packed != brka_packed && "Tickers should not match!");
    assert(aapl_packed == pack_ticker("AAPL") && "Identical tickers must map to identical integers!");

    // 6. Integration Test: Allocate an array of WALRecords in our Arena
    ArenaAllocator arena(1024);
    // Request memory for 3 records, aligned to 8 bytes
    WALRecord* records = static_cast<WALRecord*>(arena.allocate(sizeof(WALRecord) * 3, 8));

    // Calculate absolute memory addresses
    uintptr_t addr1 = reinterpret_cast<uintptr_t>(&records[0]);
    uintptr_t addr2 = reinterpret_cast<uintptr_t>(&records[1]);
    uintptr_t addr3 = reinterpret_cast<uintptr_t>(&records[2]);

    // Verify perfect contiguous packing without gaps
    assert(addr2 - addr1 == 32 && "Array spacing is incorrect, explicit padding failed!");
    assert(addr3 - addr2 == 32 && "Array spacing is incorrect, explicit padding failed!");

    std::cout << "[PASS] WALRecord memory layout and bitwise string packing are mathematically correct.\n\n";
}

int main() {
    test_arena_correctness();
    test_arena_performance();
    test_wal_record();    
    return 0;
}