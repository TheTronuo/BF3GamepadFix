#pragma once

#include <cstddef>
#include <cstdint>
#include <iterator>

namespace bf3::patch {
inline constexpr char release_version[] = "0.22.2-PC-menu-Xbox-prompts";

struct ScalarEdit {
    std::size_t offset;
    std::int32_t before;
    std::int32_t after;
    const char* field;
    const char* owner_guid;
};

// Offsets and identities are pinned to build 1147186, not guessed at run time.
struct ResourceSpec {
    const char* kind;
    const char* resource;
    const char* archive;
    std::uint64_t offset;
    std::size_t stored_size;
    const char* original_file;
    const char* patched_file;
    const char* original_sha256;
    const char* patched_sha256;
    const char* catalog_sha1;
    const char* patched_sha1;
    const ScalarEdit* edits;
    std::size_t edit_count;
    std::size_t decoded_size; // Nonzero for a compressed PC ActionScriptLibrary.
};

struct ArchiveSpec {
    const char* path;
    std::uint64_t size;
    const char* original_sha256;
    const char* patched_sha256;
};

struct MetadataSpec {
    const char* path;
    const char* sha256;
};

const ResourceSpec* release_resources() noexcept;
std::size_t release_resource_count() noexcept;
const ArchiveSpec* release_archives() noexcept;
std::size_t release_archive_count() noexcept;
const MetadataSpec* release_metadata() noexcept;
std::size_t release_metadata_count() noexcept;
} // namespace bf3::patch
