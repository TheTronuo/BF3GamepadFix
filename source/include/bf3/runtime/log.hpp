#pragma once

#include <windows.h>

namespace bf3::runtime {
bool initialize_paths(HINSTANCE module) noexcept;
const wchar_t* original_dll_path() noexcept;
const wchar_t* config_path() noexcept;
void log_line(const char* message) noexcept;
} // namespace bf3::runtime
