#pragma once

#include "bf3/bytes.hpp"
#include <filesystem>
#include <string>

namespace bf3 {
std::string sha1(const Bytes& bytes);
std::string sha256(const Bytes& bytes);
std::string sha256(const std::uint8_t* bytes, std::size_t size);
std::string file_sha256(const std::filesystem::path& path);
Bytes unhex(const std::string& text);
} // namespace bf3
