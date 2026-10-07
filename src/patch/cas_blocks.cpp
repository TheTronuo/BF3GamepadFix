#include "bf3/patch/cas_blocks.hpp"
#include <cstring>
#include <utility>
#include <zlib.h>

namespace bf3::patch {
namespace {
constexpr std::size_t block_size = 65536;

Bytes compress_block(const std::uint8_t* raw, std::size_t size, int memory_level) {
    z_stream stream{};
    if (deflateInit2(&stream, 9, Z_DEFLATED, 15, memory_level, Z_DEFAULT_STRATEGY) != Z_OK) {
        throw std::runtime_error("zlib initialization failed");
    }
    Bytes result;
    try {
        result.resize(deflateBound(&stream, static_cast<uLong>(size)));
        stream.next_in = const_cast<Bytef*>(raw);
        stream.avail_in = static_cast<uInt>(size);
        stream.next_out = result.data();
        stream.avail_out = static_cast<uInt>(result.size());
        if (deflate(&stream, Z_FINISH) != Z_STREAM_END) {
            throw std::runtime_error("zlib compression failed");
        }
        result.resize(stream.total_out);
    } catch (...) {
        deflateEnd(&stream);
        throw;
    }
    deflateEnd(&stream);
    return result;
}
} // namespace

Bytes unpack_cas(const Bytes& stored, std::size_t expected_size) {
    Bytes raw;
    raw.reserve(expected_size);
    for (std::size_t pos = 0; pos < stored.size();) {
        const auto decoded = read_be32(stored, pos);
        const auto encoded = read_be32(stored, pos + 4);
        pos += 8;
        require_range(stored, pos, encoded);
        if (!decoded || decoded > block_size || raw.size() > expected_size ||
            decoded > expected_size - raw.size()) {
            throw std::runtime_error("Invalid CAS decoded block size");
        }
        Bytes block(decoded);
        uLongf count = decoded;
        const auto status = uncompress(block.data(), &count, stored.data() + pos, encoded);
        if (status != Z_OK) {
            if (decoded != encoded)
                throw std::runtime_error("CAS decompression failed");
            std::copy_n(stored.begin() + pos, decoded, block.begin());
        } else if (count != decoded) {
            throw std::runtime_error("CAS block decoded size mismatch");
        }
        raw.insert(raw.end(), block.begin(), block.end());
        pos += encoded;
    }
    if (raw.size() != expected_size)
        throw std::runtime_error("CAS resource decoded size mismatch");
    return raw;
}

Bytes pack_cas_fixed(const Bytes& raw, std::size_t stored_size) {
    // The pinned release uses zlib 1.3.1. Compression drift must fail explicitly,
    // rather than produce bytes that the DLL's exact whitelist will not accept.
    if (std::strcmp(zlibVersion(), "1.3.1"))
        throw std::runtime_error("Use zlib 1.3.1 for this release");
    if (raw.empty())
        throw std::runtime_error("Empty CAS resource");
    struct Block {
        std::size_t decoded;
        Bytes compressed;
    };
    std::vector<Block> blocks;
    std::size_t used = 0;
    for (std::size_t pos = 0; pos < raw.size(); pos += block_size) {
        const auto size = (std::min)(block_size, raw.size() - pos);
        Bytes best;
        for (int memory_level = 5; memory_level <= 9; ++memory_level) {
            auto candidate = compress_block(raw.data() + pos, size, memory_level);
            if (best.empty() || candidate.size() < best.size())
                best = std::move(candidate);
        }
        used += 8 + best.size();
        blocks.push_back({size, std::move(best)});
    }
    if (used > stored_size)
        throw std::runtime_error("Fixed CAS size budget exceeded");
    blocks.back().compressed.resize(blocks.back().compressed.size() + stored_size - used, 0);
    Bytes result;
    result.reserve(stored_size);
    for (const auto& block : blocks) {
        append_be32(result, static_cast<std::uint32_t>(block.decoded));
        append_be32(result, static_cast<std::uint32_t>(block.compressed.size()));
        result.insert(result.end(), block.compressed.begin(), block.compressed.end());
    }
    if (unpack_cas(result, raw.size()) != raw)
        throw std::runtime_error("CAS roundtrip mismatch");
    return result;
}
} // namespace bf3::patch
