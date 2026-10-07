#include "bf3/patch/prompt_widgets.hpp"
#include <array>
#include <iterator>
#include <set>
#include <string_view>

namespace bf3::patch {
namespace {
struct Widget {
    std::array<const char*, 4> tokens;
    std::size_t token_count;
    const char* field;
};
constexpr Widget widgets[] = {
    {{{"Widget", "Hud", "QuickTimeEvent", ""}}, 3, "m_platform"},
    {{{"Widget", "Icon3D", "Interaction", "InteractionTag"}}, 4, "m_platform"},
    {{{"Widget", "Message", "TooltipMessage", ""}}, 3, "m_platform"},
    {{{"Widget", "Button", "ConsoleButtonBar", ""}}, 3, "m_Platform"},
};

std::string cstring(const Bytes& bytes, std::size_t& pos, std::size_t end) {
    auto begin = pos;
    while (pos < end && bytes[pos])
        ++pos;
    if (pos == end)
        throw std::runtime_error("Unterminated AVM1 constant");
    std::string value(bytes.begin() + begin, bytes.begin() + pos);
    ++pos;
    return value;
}

void pool_operand(Bytes& body, const std::vector<std::string>& pool, const std::string& value) {
    const auto found = std::find(pool.begin(), pool.end(), value);
    if (found == pool.end()) {
        body.push_back(0); // AVM1 literal string.
        body.insert(body.end(), value.begin(), value.end());
        body.push_back(0);
    } else {
        const auto index = static_cast<std::size_t>(found - pool.begin());
        if (index < 256) {
            body.push_back(8);
            body.push_back(static_cast<std::uint8_t>(index));
        } else {
            body.push_back(9);
            append_u16(body, static_cast<std::uint16_t>(index));
        }
    }
}

void push(Bytes& code, const Bytes& body) {
    code.push_back(0x96);
    append_u16(code, static_cast<std::uint16_t>(body.size()));
    code.insert(code.end(), body.begin(), body.end());
}

Bytes assignment(const std::vector<std::string>& pool, const char* field, bool xbox,
                 bool ps3 = false) {
    Bytes code, body{4, 1}; // Push register 1 (the widget instance).
    pool_operand(body, pool, field);
    if (xbox) {
        pool_operand(body, pool, ps3 ? "ps3" : "xenon");
        push(code, body);
        code.push_back(0x4f); // SetMember.
        push(code, {4, 1});
        code.push_back(0x17); // Pop.
        // The PS3 literal is two bytes shorter. An extra undefined/Pop pair
        // retains the original 28-byte assignment and all branch offsets.
        push(code, ps3 ? Bytes{3, 3, 3} : Bytes{3, 3});
        code.insert(code.end(), ps3 ? 3 : 2, 0x17);
    } else {
        body.push_back(7);
        append_u32(body, 0); // Call argument count.
        pool_operand(body, pool, "fb");
        push(code, body);
        code.push_back(0x1c); // GetVariable.
        body.clear();
        pool_operand(body, pool, "HelperFunctions");
        push(code, body);
        code.push_back(0x4e); // GetMember.
        body.clear();
        pool_operand(body, pool, "getPlatform");
        push(code, body);
        code.insert(code.end(), {0x52, 0x4f}); // CallMethod, SetMember.
    }
    return code;
}
PromptLibrary patch_library(const Bytes& original, bool ps3) {
    require_range(original, 0, 9);
    if (std::string(original.begin(), original.begin() + 3) != "GFX" || original[3] != 8 ||
        read_u32(original, 4) != original.size()) {
        throw std::runtime_error("Unsupported PC GFx movie");
    }
    const auto rectangle_bytes = (5 + 4 * (original[8] >> 3) + 7) / 8;
    std::size_t pos = 8 + rectangle_bytes + 4;
    require_range(original, pos, 0);
    PromptLibrary result{original, {}};
    std::set<std::size_t> seen;
    bool ended = false;
    while (pos < original.size()) {
        const auto header = read_u16(original, pos);
        pos += 2;
        const auto tag = header >> 6;
        std::size_t length = header & 63;
        if (length == 63) {
            length = read_u32(original, pos);
            pos += 4;
        }
        require_range(original, pos, length);
        const auto end = pos + length;
        if (tag == 0) {
            ended = true;
            break;
        }
        // DoInitAction: sprite ID followed by ActionConstantPool.
        if (tag == 59 && length >= 7 && original[pos + 2] == 0x88) {
            const auto pool_size = read_u16(original, pos + 3);
            const auto count = read_u16(original, pos + 5);
            if (pool_size < 2 || pool_size > length - 5)
                throw std::runtime_error("Invalid AVM1 pool size");
            const auto pool_end = pos + 5 + pool_size;
            auto cursor = pos + 7;
            std::vector<std::string> pool;
            for (unsigned i = 0; i < count; ++i)
                pool.push_back(cstring(original, cursor, pool_end));
            if (cursor != pool_end)
                throw std::runtime_error("AVM1 pool length mismatch");
            for (std::size_t i = 0; i < std::size(widgets); ++i) {
                const auto& widget = widgets[i];
                if (pool.size() < widget.token_count ||
                    !std::equal(widget.tokens.begin(), widget.tokens.begin() + widget.token_count,
                                pool.begin()))
                    continue;
                if (!seen.insert(i).second)
                    throw std::runtime_error("Duplicate prompt class");
                const auto before = assignment(pool, widget.field, ps3);
                const auto after = assignment(pool, widget.field, true, ps3);
                if (before.size() != 28 || after.size() != before.size()) {
                    throw std::runtime_error("AVM1 assignment would change branch offsets");
                }
                const auto first = original.begin() + pos;
                const auto last = original.begin() + end;
                const auto found = std::search(first, last, before.begin(), before.end());
                if (found == last ||
                    std::search(found + 1, last, before.begin(), before.end()) != last) {
                    throw std::runtime_error(
                        "Expected exactly one platform assignment per prompt class");
                }
                const auto offset = static_cast<std::size_t>(found - original.begin());
                std::copy(after.begin(), after.end(), result.bytes.begin() + offset);
                std::string class_name;
                for (std::size_t token = 0; token < widget.token_count; ++token) {
                    if (token)
                        class_name += '.';
                    class_name += widget.tokens[token];
                }
                result.edits.push_back({class_name, widget.field, offset, before, after});
                if (ps3 && i == 0) {
                    // The original PS3 QTE dictionary swaps some shoulder and
                    // trigger IDs. Input remains Xbox, so use its dictionary for
                    // both presentations, without changing any input actions.
                    auto lookup = [&](const char* dictionary) {
                        Bytes bytes, operand{4, 1};
                        pool_operand(operand, pool, dictionary);
                        push(bytes, operand);
                        bytes.push_back(0x4e);
                        push(bytes, {4, 2});
                        bytes.push_back(0x4e);
                        return bytes;
                    };
                    const auto old_lookup = lookup("m_gamepadTranslation");
                    const auto new_lookup = lookup("m_gamepadTranslationXenon");
                    const auto location =
                        std::search(first, last, old_lookup.begin(), old_lookup.end());
                    if (old_lookup.size() != new_lookup.size() || location == last ||
                        std::search(location + 1, last, old_lookup.begin(), old_lookup.end()) !=
                            last)
                        throw std::runtime_error("Expected exactly one PS3 QTE dictionary lookup");
                    const auto qte_offset = static_cast<std::size_t>(location - original.begin());
                    std::copy(new_lookup.begin(), new_lookup.end(),
                              result.bytes.begin() + qte_offset);
                    result.edits.push_back({class_name, "QTE physical-button dictionary",
                                            qte_offset, old_lookup, new_lookup});
                }
            }
        }
        pos = end;
    }
    if (!ended || seen.size() != std::size(widgets))
        throw std::runtime_error("Missing GFx end tag or prompt classes");
    // All writes were bounded, equal-length assignments. Shared platform getter,
    // options pages, mouse handlers and the remaining movie bytes stay identical.
    return result;
}
} // namespace

PromptLibrary patch_prompt_widgets(const Bytes& original) {
    return patch_library(original, false);
}

PromptLibrary switch_prompt_widgets_to_ps3(const Bytes& xbox) {
    return patch_library(xbox, true);
}
} // namespace bf3::patch
