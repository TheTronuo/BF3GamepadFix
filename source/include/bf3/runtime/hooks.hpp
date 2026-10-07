#pragma once

#include <windows.h>

namespace bf3::runtime {
bool install_hooks(BYTE* image) noexcept;
void initialize_runtime() noexcept;
} // namespace bf3::runtime
