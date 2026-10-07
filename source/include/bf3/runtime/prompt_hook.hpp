#pragma once
#include <windows.h>

namespace bf3::runtime {
// Called only for an explicit PS3 config, after the existing Xbox hooks succeed.
bool install_ps3_prompt_hook(BYTE* image) noexcept;
bool has_expected_prompt_target(const BYTE* image, DWORD size) noexcept;
} // namespace bf3::runtime
