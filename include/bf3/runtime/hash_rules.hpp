#pragma once

#include <cstddef>

namespace bf3::runtime {
struct HashRule {
    unsigned char modified[20];
    unsigned char original[20];
    const char* name;
};
const HashRule* hash_rules() noexcept;
std::size_t hash_rule_count() noexcept;
unsigned apply_hash_rule(unsigned char* digest) noexcept;
} // namespace bf3::runtime
