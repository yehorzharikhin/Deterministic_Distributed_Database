#pragma once
#include <liburing.h>
#include <cstdint>

class AsyncDiskWriter {
private:
    struct io_uring ring;
    int file_fd;
    uint32_t unsubmitted_count = 0;
    uint64_t last_submit_tsc = 0;

    inline uint64_t rdtsc() {
        unsigned int lo, hi;
        __asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi));
        return ((uint64_t)hi << 32) | lo;
    }

public:
    AsyncDiskWriter(const char* filepath);
    ~AsyncDiskWriter();

    // HOT PATH
    inline void queue_write(void* memory_ptr, size_t length, uint64_t disk_offset) {
        struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
        if (!sqe) {
            flush_to_kernel();
            sqe = io_uring_get_sqe(&ring);
        }
        
        io_uring_prep_write(sqe, file_fd, memory_ptr, length, disk_offset);
        io_uring_sqe_set_data(sqe, memory_ptr);
        unsubmitted_count++;
        
        if (unsubmitted_count >= 32 || (rdtsc() - last_submit_tsc > 400'000)) {
            flush_to_kernel();
        }
    }

    inline void flush_to_kernel() {
        if (unsubmitted_count > 0) {
            io_uring_submit(&ring);
            unsubmitted_count = 0;
            last_submit_tsc = rdtsc();
        }
    }

    template <typename Func>
    inline void poll_completions(Func on_completion) {
        struct io_uring_cqe *cqe;
        unsigned head;
        unsigned count = 0;

        io_uring_for_each_cqe(&ring, head, cqe) {
            if (cqe->res >= 0) {
                on_completion(io_uring_cqe_get_data(cqe));
            }
            count++;
        }

        if (count > 0) io_uring_cq_advance(&ring, count);
    }
};