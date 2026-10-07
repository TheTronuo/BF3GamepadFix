#include "bf3/runtime/hash_rules.hpp"
#include <cstring>
#include <iterator>

namespace bf3::runtime {
namespace {
#include "hash_rules_data.inc"
} // namespace

const HashRule* hash_rules() noexcept {
    return hash_rule_data;
}
std::size_t hash_rule_count() noexcept {
    return std::size(hash_rule_data);
}

unsigned apply_hash_rule(unsigned char* digest) noexcept {
    if (!digest)
        return 0;
    for (std::size_t i = 0; i < hash_rule_count(); ++i) {
        if (!std::memcmp(digest, hash_rule_data[i].modified, 20)) {
            std::memcpy(digest, hash_rule_data[i].original, 20);
            return static_cast<unsigned>(i + 1);
        }
    }
    return 0;
}
} // namespace bf3::runtime
