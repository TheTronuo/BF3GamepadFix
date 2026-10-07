#pragma once

#include "bf3/bytes.hpp"

namespace bf3::patch {
Bytes unpack_cas(const Bytes& stored, std::size_t expected_size);
Bytes pack_cas_fixed(const Bytes& raw, std::size_t stored_size);
} // namespace bf3::patch
