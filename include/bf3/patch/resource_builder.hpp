#pragma once

#include "bf3/patch/prompt_widgets.hpp"
#include "bf3/patch/release.hpp"
#include <filesystem>
#include <string>

namespace bf3::patch {
struct BuiltResource {
    Bytes bytes;
    std::vector<PromptEdit> prompt_edits;
};

BuiltResource rebuild_resource(const ResourceSpec& spec, const Bytes& original);
Bytes read_resource(const std::filesystem::path& path, std::uint64_t offset, std::size_t size);
void write_bytes(const std::filesystem::path& path, const Bytes& bytes);
std::string generate_hash_rules();
std::string generate_manifest();

// Verifies the original game archives and signed metadata without modifying them.
void verify_original_game(const std::filesystem::path& game);
} // namespace bf3::patch
