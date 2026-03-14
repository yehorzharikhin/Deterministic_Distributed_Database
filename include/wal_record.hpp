#pragma once
#include <cstdint>
#include <cstring>
#include <string>
#include <algorithm>

struct alignas(8) WALRecord {
    uint64_t timestamp;
    int64_t  price;
    uint64_t ticker;
    uint32_t volume;
    uint32_t _padding;
};

inline uint64_t pack_ticker(const char* str) {
    uint64_t result = 0;
    std::memcpy(&result, str, std::min(std::strlen(str), size_t(8)));
    return result;
}

// debug only
inline std::string unpack_ticker(uint64_t ticker_val) {
    char buf[9] = {0};
    std::memcpy(buf, &ticker_val, 8);
    return std::string(buf);
}