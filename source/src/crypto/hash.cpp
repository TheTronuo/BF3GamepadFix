#include <windows.h>
#include "bf3/hash.hpp"
#include <array>
#include <bcrypt.h>
#include <fstream>
#include <limits>
#include <sstream>

namespace bf3 {
namespace {
void check(NTSTATUS status) {
    if (status < 0)
        throw std::runtime_error("Windows CNG hash operation failed");
}

// Handles are owned here so exceptions during file reads also release CNG state.
class Hash {
  public:
    explicit Hash(LPCWSTR algorithm) {
        check(BCryptOpenAlgorithmProvider(&algorithm_, algorithm, nullptr, 0));
        try {
            DWORD length = 0, received = 0;
            check(BCryptGetProperty(algorithm_, BCRYPT_HASH_LENGTH,
                                    reinterpret_cast<PUCHAR>(&length), sizeof(length), &received,
                                    0));
            digest_.resize(length);
            check(BCryptCreateHash(algorithm_, &hash_, nullptr, 0, nullptr, 0, 0));
        } catch (...) {
            BCryptCloseAlgorithmProvider(algorithm_, 0);
            throw;
        }
    }
    ~Hash() {
        if (hash_)
            BCryptDestroyHash(hash_);
        if (algorithm_)
            BCryptCloseAlgorithmProvider(algorithm_, 0);
    }
    Hash(const Hash&) = delete;
    Hash& operator=(const Hash&) = delete;

    void update(const std::uint8_t* data, std::size_t size) {
        while (size) {
            const auto count = static_cast<ULONG>((std::min)(size, std::size_t(1 << 20)));
            check(BCryptHashData(hash_, const_cast<PUCHAR>(data), count, 0));
            data += count;
            size -= count;
        }
    }
    std::string finish() {
        check(BCryptFinishHash(hash_, digest_.data(), static_cast<ULONG>(digest_.size()), 0));
        constexpr char digits[] = "0123456789abcdef";
        std::string text;
        for (auto byte : digest_) {
            text += digits[byte >> 4];
            text += digits[byte & 15];
        }
        return text;
    }

  private:
    BCRYPT_ALG_HANDLE algorithm_{};
    BCRYPT_HASH_HANDLE hash_{};
    Bytes digest_;
};
} // namespace

std::string sha1(const Bytes& bytes) {
    Hash hash(BCRYPT_SHA1_ALGORITHM);
    hash.update(bytes.data(), bytes.size());
    return hash.finish();
}

std::string sha256(const Bytes& bytes) {
    return sha256(bytes.data(), bytes.size());
}

std::string sha256(const std::uint8_t* bytes, std::size_t size) {
    Hash hash(BCRYPT_SHA256_ALGORITHM);
    hash.update(bytes, size);
    return hash.finish();
}

std::string file_sha256(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file)
        throw std::runtime_error("Cannot read file: " + path.string());
    Hash hash(BCRYPT_SHA256_ALGORITHM);
    std::array<std::uint8_t, 65536> block{};
    while (file.read(reinterpret_cast<char*>(block.data()), block.size()) || file.gcount()) {
        hash.update(block.data(), static_cast<std::size_t>(file.gcount()));
    }
    if (!file.eof())
        throw std::runtime_error("File read failed: " + path.string());
    return hash.finish();
}

Bytes unhex(const std::string& text) {
    if (text.size() % 2)
        throw std::runtime_error("Odd hex string length");
    auto digit = [](char c) -> unsigned {
        if (c >= '0' && c <= '9')
            return c - '0';
        if (c >= 'a' && c <= 'f')
            return c - 'a' + 10;
        if (c >= 'A' && c <= 'F')
            return c - 'A' + 10;
        throw std::runtime_error("Invalid hex digit");
    };
    Bytes result;
    for (std::size_t i = 0; i < text.size(); i += 2) {
        result.push_back(static_cast<std::uint8_t>((digit(text[i]) << 4) | digit(text[i + 1])));
    }
    return result;
}
} // namespace bf3
