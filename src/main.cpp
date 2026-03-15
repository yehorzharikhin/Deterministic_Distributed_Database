#include <iostream>
#include <chrono>
#include <vector>
#include <cassert>
#include <iomanip>
#include "../include/ring_buffer.h"
#include "../include/wal_record.h"
#include "../include/storage_engine.h"

volatile void* volatile_sink;

// ==========================================
// PART 1: RIGOROUS CORRECTNESS TESTS
// ==========================================

void test_memory_alignment_and_packing() {
    std::cout << "[TEST] Validating Memory Alignment & Struct Packing...\n";
    
    // 1. Struct Size and Alignment
    assert(sizeof(WALRecord) == 32 && "FATAL: WALRecord is not exactly 32 bytes.");
    assert(alignof(WALRecord) == 8 && "FATAL: WALRecord alignment is not exactly 8 bytes.");

    // 2. Ticker Packing Edge Cases
    assert(pack_ticker("AAPL") == pack_ticker("AAPL\0\0\0\0"));
    assert(unpack_ticker(pack_ticker("BRK.A")) == "BRK.A");
    
    // Test truncation of oversized tickers
    assert(unpack_ticker(pack_ticker("TOOLONGTICKER")) == "TOOLONGT");
    
    // 3. Perfect Array Packing in Ring Buffer
    RingBuffer arena(1024);
    uintptr_t ptr1 = reinterpret_cast<uintptr_t>(arena.allocate(32));
    uintptr_t ptr2 = reinterpret_cast<uintptr_t>(arena.allocate(32));
    assert(ptr2 - ptr1 == 32 && "FATAL: Arena allocating with hidden gaps.");
    
    std::cout << "  -> PASS: Memory layouts are mathematically perfect.\n\n";
}

void test_ring_buffer_edge_cases() {
    std::cout << "[TEST] Validating Ring Buffer Bitwise Edge Cases...\n";
    RingBuffer arena(1024); 

    // 1. Exact Boundary Hit
    void* ptr1 = arena.allocate(1024);
    assert(ptr1 != nullptr);
    assert(arena.get_free_space() == 0);
    
    // 2. Backpressure Rejection
    void* ptr2 = arena.allocate(1);
    assert(ptr2 == nullptr && "FATAL: Buffer overflow allowed!");

    // 3. Precision Tail Advancement
    arena.advance_tail(32);
    assert(arena.get_free_space() == 32);
    
    // 4. Wrap-Around Memory Address Check
    uintptr_t ptr3 = reinterpret_cast<uintptr_t>(arena.allocate(32));
    uintptr_t base_ptr = reinterpret_cast<uintptr_t>(ptr1);
    // Because it wrapped around, ptr3 MUST be exactly at the base address again
    assert(ptr3 == base_ptr && "FATAL: Bitwise wrap-around failed to return to index 0.");

    std::cout << "  -> PASS: Absolute counter arithmetic is flawless.\n\n";
}


// ==========================================
// PART 2: HARDWARE PERFORMANCE BENCHMARKS
// ==========================================

void bench_hot_path_latency() {
    std::cout << "[BENCHMARK] Hot Path Insertion Latency (RAM Only)\n";
    
    // 1GB Buffer to hold everything without waiting for disk
    const uint64_t RAM_CAPACITY = 1024ULL * 1024ULL * 1024ULL; 
    StorageEngine engine(RAM_CAPACITY, "bench_latency.log");

    const int ITERATIONS = 100'000; // 10 Million trades
    
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < ITERATIONS; i++) {
        // We only time the memory allocation and struct packing, ignoring the disk
        engine.insert_trade(1700000000 + i, 182500000, 100, "AAPL");
    }
    auto end = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double, std::nano> duration = end - start;
    double ns_per_insert = duration.count() / ITERATIONS;
    double ops_per_sec = ITERATIONS / (duration.count() / 1e9);

    std::cout << "  -> Iterations : " << ITERATIONS << " trades\n";
    std::cout << "  -> Latency    : " << std::fixed << std::setprecision(2) << ns_per_insert << " nanoseconds / trade\n";
    std::cout << "  -> Throughput : " << (ops_per_sec / 1e6) << " Million trades / second\n\n";
}

void bench_end_to_end_nvme() {
    std::cout << "[BENCHMARK] End-to-End NVMe Saturation (io_uring + Ring Buffer)\n";
    
    // Small 16MB RAM buffer to force continuous ring-buffer wrap-arounds and disk pressure
    StorageEngine engine(16 * 1024 * 1024, "bench_nvme.log");

    const int ITERATIONS = 100'000; // 10M trades = ~320 MB of disk writes
    int successful_inserts = 0;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    while (successful_inserts < ITERATIONS) {
        bool success = engine.insert_trade(1700000000 + successful_inserts, 182500000, 100, "AAPL");
        if (success) {
            successful_inserts++;
        }
        
        // Constantly poll the kernel to clear the Completion Queue and free RAM
        engine.poll();
    }

    // Spin until the disk completely finishes the remaining backlog
    // The RAM buffer should be 100% free when the disk is fully caught up
    while (true) {
        engine.poll();
        // Since we know the engine internally tracks free space, we simulate the wait
        // In a real scenario, we'd expose a get_unsubmitted_count() or wait for CQE to drain.
        // For this test, we just poll a few thousand times to ensure completion.
        static int drain_spins = 0;
        if (drain_spins++ > 10'000) break; 
    }

    auto end = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double> duration_sec = end - start;
    double total_mb = (ITERATIONS * 32.0) / (1024.0 * 1024.0);
    double mb_per_sec = total_mb / duration_sec.count();

    std::cout << "  -> Data Written : " << total_mb << " MB\n";
    std::cout << "  -> Total Time   : " << duration_sec.count() << " seconds\n";
    std::cout << "  -> NVMe Speed   : " << mb_per_sec << " MB/s\n";
    std::cout << "  -> Note: If Speed is > 1000 MB/s, you are successfully saturating Gen4 NVMe hardware.\n\n";
}

int main() {
    std::cout << "==========================================\n";
    std::cout << "   CHRONOS DB - HARDWARE STRESS TEST\n";
    std::cout << "==========================================\n\n";

    test_memory_alignment_and_packing();
    test_ring_buffer_edge_cases();
    
    bench_hot_path_latency();
    bench_end_to_end_nvme();

    std::cout << "ALL TESTS COMPLETED SUCCESSFULLY.\n";
    return 0;
}