#include "bf3/runtime/config.hpp"
#include <iterator>

namespace bf3::runtime {
namespace {
bool equals_ascii(std::wstring_view value, std::wstring_view expected) noexcept {
    if (value.size() != expected.size())
        return false;
    for (std::size_t i = 0; i < value.size(); ++i) {
        auto ch = value[i];
        if (ch >= L'A' && ch <= L'Z')
            ch += L'a' - L'A';
        if (ch != expected[i])
            return false;
    }
    return true;
}
} // namespace

PromptConfig parse_prompt_style(std::wstring_view value) noexcept {
    while (!value.empty() && (value.front() == L' ' || value.front() == L'\t'))
        value.remove_prefix(1);
    while (!value.empty() && (value.back() == L' ' || value.back() == L'\t'))
        value.remove_suffix(1);
    if (equals_ascii(value, L"xbox360"))
        return {PromptStyle::Xbox360, true};
    if (equals_ascii(value, L"ps3"))
        return {PromptStyle::PS3, true};
    return {PromptStyle::Xbox360, false};
}

PromptConfig read_prompt_config(const wchar_t* path) noexcept {
    wchar_t value[64]{};
    if (!path || !*path)
        return {};
    const auto count = GetPrivateProfileStringW(L"Controller", L"PromptStyle", L"Xbox360", value,
                                                static_cast<DWORD>(std::size(value)), path);
    if (count >= std::size(value) - 1)
        return {PromptStyle::Xbox360, false};
    return parse_prompt_style({value, count});
}
} // namespace bf3::runtime
