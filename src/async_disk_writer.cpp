#include "../include/async_disk_writer.h"
#include <fcntl.h>
#include <unistd.h>
#include <stdexcept>

AsyncDiskWriter::AsyncDiskWriter(const char* filepath) {
    file_fd = open(filepath, O_WRONLY | O_CREAT | O_APPEND | O_DSYNC, 0644);
    if (file_fd < 0) throw std::runtime_error("Failed to open WAL file");
    
    if (io_uring_queue_init(1024, &ring, 0) < 0) {
        close(file_fd);
        throw std::runtime_error("Failed to initialize io_uring");
    }
}

AsyncDiskWriter::~AsyncDiskWriter() {
    flush_to_kernel();
    io_uring_queue_exit(&ring);
    close(file_fd);
}