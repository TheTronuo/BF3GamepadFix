#pragma once

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace bf3 {
using Bytes = std::vector<std::uint8_t>;

inline void require_range(const Bytes& bytes, std::size_t offset, std::size_t length) {
    if (offset > bytes.size() || length > bytes.size() - offset) {
        throw std::runtime_error("Truncated binary data");
    }
}

inline std::uint16_t read_u16(const Bytes& bytes, std::size_t offset) {
    require_range(bytes, offset, 2);
    return static_cast<std::uint16_t>(bytes[offset] | (bytes[offset + 1] << 8));
}

inline std::uint32_t read_u32(const Bytes& bytes, std::size_t offset) {
    require_range(bytes, offset, 4);
    std::uint32_t result = 0;
    for (unsigned i = 0; i < 4; ++i)
        result |= std::uint32_t(bytes[offset + i]) << (8 * i);
    return result;
}

inline std::uint32_t read_be32(const Bytes& bytes, std::size_t offset) {
    require_range(bytes, offset, 4);
    std::uint32_t result = 0;
    for (unsigned i = 0; i < 4; ++i)
        result = (result << 8) | bytes[offset + i];
    return result;
}

inline void append_u16(Bytes& bytes, std::uint16_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value));
    bytes.push_back(static_cast<std::uint8_t>(value >> 8));
}

inline void append_u32(Bytes& bytes, std::uint32_t value) {
    for (unsigned i = 0; i < 4; ++i)
        bytes.push_back(static_cast<std::uint8_t>(value >> (8 * i)));
}

inline void append_be32(Bytes& bytes, std::uint32_t value) {
    for (unsigned i = 0; i < 4; ++i)
        bytes.push_back(static_cast<std::uint8_t>(value >> (8 * (3 - i))));
}
} // namespace bf3
