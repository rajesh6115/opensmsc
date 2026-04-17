#pragma once

#include <chrono>
#include <iomanip>
#include <random>
#include <sstream>
#include <string>

// Generates a unique, URL-safe session ID:
//   <timestamp_ms_hex>-<64-bit_random_hex>
// Example: 0195f3a7b2c1-a3f8d901c4b72e5d
inline std::string generate_session_id()
{
    // High-resolution timestamp component
    auto now = std::chrono::high_resolution_clock::now();
    auto ts  = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count());

    // Random component
    thread_local std::mt19937_64 rng{std::random_device{}()};
    std::uniform_int_distribution<uint64_t> dist;
    uint64_t rnd = dist(rng);

    std::ostringstream oss;
    oss << std::hex << std::setfill('0')
        << std::setw(12) << (ts & 0x0000FFFFFFFFFFFF)   // 48-bit ts
        << '-'
        << std::setw(16) << rnd;                          // 64-bit random
    return oss.str();
}
