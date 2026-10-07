#include "bf3/runtime/engine.hpp"
#include "bf3/runtime/prompt_hook.hpp"
#include <cstring>

namespace bf3::runtime {
bool has_expected_prompt_target(const BYTE* image, DWORD size) noexcept {
    // Native GFx memory-file constructor: this, name, borrowed data, byte count;
    // returns this and pops three arguments (ret 0Ch).
    constexpr BYTE prefix[] = {0x56, 0x8b, 0xf1, 0x57, 0x8b, 0x7c, 0x24,
                               0x0c, 0xc7, 0x06, 0x88, 0xaa, 0x0a, 0x02};
    constexpr BYTE body[] = {0x8b, 0x44, 0x24, 0x10, 0x8b, 0x4c, 0x24,
                             0x14, 0x89, 0x46, 0x0c, 0x89, 0x4e, 0x10};
    constexpr BYTE tail[] = {0x5f, 0x8b, 0xc6, 0x5e, 0xc2, 0x0c, 0x00};
    return image && size == address::image_size &&
           !std::memcmp(image + address::memory_file_ctor, prefix, sizeof(prefix)) &&
           !std::memcmp(image + address::memory_file_ctor + 0x24, body, sizeof(body)) &&
           !std::memcmp(image + address::memory_file_ctor + 0x4a, tail, sizeof(tail));
}

bool is_supported_executable(const BYTE* base) noexcept {
    if (base != reinterpret_cast<const BYTE*>(0x00400000))
        return false;
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew < 0 || dos->e_lfanew > 4096)
        return false;
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(base + dos->e_lfanew);
    return nt->Signature == IMAGE_NT_SIGNATURE &&
           nt->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC &&
           nt->FileHeader.Machine == IMAGE_FILE_MACHINE_I386 &&
           nt->FileHeader.TimeDateStamp == address::timestamp &&
           nt->OptionalHeader.SizeOfImage == address::image_size;
}

bool is_win32_ui(const BYTE* base, DWORD size) noexcept {
    constexpr BYTE instruction[] = {0x68, 0x8c, 0x0f, 0x07, 0x02};
    return base && size >= address::win32_string + 6 &&
           !std::memcmp(base + address::platform_instruction, instruction, sizeof(instruction)) &&
           !std::memcmp(base + address::win32_string, "win32", 6);
}

bool has_expected_hook_targets(const BYTE* base, DWORD size) noexcept {
    constexpr BYTE sha_prefix[] = {0x53, 0x56, 0x57, 0x8b, 0x7c, 0x24, 0x14, 0x8b, 0x5f,
                                   0x5c, 0x8d, 0x77, 0x1c, 0xc6, 0x04, 0x33, 0x80};
    constexpr BYTE text_prefix[] = {0x8b, 0x4c, 0x24, 0x08, 0x83, 0xec,
                                    0x14, 0x33, 0xc0, 0x56, 0x3b, 0xc8};
    constexpr BYTE ctor_prefix[] = {0xe9, 0xf8, 0x74, 0x00, 0x00};
    return is_win32_ui(base, size) && size == address::image_size &&
           !std::memcmp(base + address::sha_final, sha_prefix, sizeof(sha_prefix)) &&
           !std::memcmp(base + address::binding_text, text_prefix, sizeof(text_prefix)) &&
           !std::memcmp(base + address::string_ctor, ctor_prefix, sizeof(ctor_prefix)) &&
           !std::memcmp(base + address::xenon_string, "xenon", 6);
}
} // namespace bf3::runtime
