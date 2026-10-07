#pragma once

#include "bf3/bytes.hpp"
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace bf3::install {
struct RangeChange {
    std::uint64_t offset;
    Bytes before;
    Bytes after;
};

struct ArchiveChange {
    std::filesystem::path path;
    std::uint64_t size;
    std::string before_sha256;
    std::string after_sha256;
    std::vector<RangeChange> ranges;
};

struct FileChange {
    std::filesystem::path path;
    bool existed;
    Bytes before;
    Bytes after;
};

// Locks and validates every archive before the first write. A failed operation
// restores all archive ranges and replaced files to their exact previous bytes.
void apply_transaction(const std::vector<ArchiveChange>& archives,
                       const std::vector<FileChange>& files,
                       const std::function<void()>& after_range_write = {});

std::filesystem::path contained_path(const std::filesystem::path& root,
                                     const std::filesystem::path& relative);
} // namespace bf3::install
