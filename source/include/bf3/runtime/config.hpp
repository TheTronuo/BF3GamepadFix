#pragma once

#include <windows.h>
#include <string_view>

namespace bf3::runtime {
enum class PromptStyle { Xbox360, PS3 };
struct PromptConfig {
    PromptStyle style = PromptStyle::Xbox360;
    bool valid = true;
};

PromptConfig parse_prompt_style(std::wstring_view value) noexcept;
PromptConfig read_prompt_config(const wchar_t* path) noexcept;
} // namespace bf3::runtime
