#pragma once

#include <windows.h>

namespace bf3::runtime {
namespace address {
inline constexpr DWORD image_size = 34119680;
inline constexpr DWORD timestamp = 0x511c9356;
inline constexpr DWORD sha_final = 0xebdd40;
inline constexpr DWORD binding_text = 0x50c550;
inline constexpr DWORD string_ctor = 0x21f3;
inline constexpr DWORD pad_type_first = 0x1ff604c;
inline constexpr DWORD pad_type_last = 0x1ff604e;
inline constexpr DWORD platform_instruction = 0x46540b;
inline constexpr DWORD win32_string = 0x1c70f8c;
inline constexpr DWORD xenon_string = 0x1c70f84;
inline constexpr DWORD memory_file_ctor = 0x1367f00;
} // namespace address

bool is_supported_executable(const BYTE* base) noexcept;
bool is_win32_ui(const BYTE* base, DWORD size) noexcept;
bool has_expected_hook_targets(const BYTE* base, DWORD size) noexcept;
} // namespace bf3::runtime
