#include "bf3/runtime/config.hpp"
#include "bf3/runtime/engine.hpp"
#include "bf3/runtime/hash_rules.hpp"
#include "bf3/runtime/input_frames.hpp"
#include "bf3/runtime/prompt_hook.hpp"
#include "bf3/runtime/ui_scaling.hpp"
#include <array>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace runtime = bf3::runtime;
namespace {
void expect(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}
std::array<BYTE, 24> pad_type{}, keyboard_type{};
BYTE* __fastcall pad_get_type(BYTE*, void*) {
    return pad_type.data();
}
BYTE* __fastcall keyboard_get_type(BYTE*, void*) {
    return keyboard_type.data();
}

struct VirtualFreeDeleter {
    void operator()(BYTE* pointer) const { VirtualFree(pointer, 0, MEM_RELEASE); }
};
using Image = std::unique_ptr<BYTE, VirtualFreeDeleter>;
} // namespace

int main() {
    try {
        for (const auto* value : {L"Xbox360", L"xbox360", L"  XBOX360\t"}) {
            const auto config = runtime::parse_prompt_style(value);
            expect(config.valid && config.style == runtime::PromptStyle::Xbox360,
                   "Xbox config parsing");
        }
        for (const auto* value : {L"PS3", L"ps3", L"\tPs3 "}) {
            const auto config = runtime::parse_prompt_style(value);
            expect(config.valid && config.style == runtime::PromptStyle::PS3, "PS3 config parsing");
        }
        for (const auto* value : {L"", L"PS4", L"PS3garbage", L"Xbox"}) {
            const auto config = runtime::parse_prompt_style(value);
            expect(!config.valid && config.style == runtime::PromptStyle::Xbox360,
                   "Invalid config must default to Xbox");
        }
        wchar_t temporary_directory[MAX_PATH]{}, temporary_file[MAX_PATH]{};
        expect(GetTempPathW(MAX_PATH, temporary_directory) &&
                   GetTempFileNameW(temporary_directory, L"bf3", 0, temporary_file),
               "Config fixture creation failed");
        expect(runtime::read_prompt_config(temporary_file).style == runtime::PromptStyle::Xbox360,
               "Missing setting defaults to Xbox");
        expect(WritePrivateProfileStringW(L"Controller", L"PromptStyle", L"PS3", temporary_file),
               "Config fixture write failed");
        expect(runtime::read_prompt_config(temporary_file).style == runtime::PromptStyle::PS3,
               "INI selection not read");
        expect(WritePrivateProfileStringW(L"Controller", L"PromptStyle", L"TYPO", temporary_file),
               "Config fixture write failed");
        const auto invalid_config = runtime::read_prompt_config(temporary_file);
        expect(!invalid_config.valid && invalid_config.style == runtime::PromptStyle::Xbox360,
               "Invalid INI did not fall back");
        expect(DeleteFileW(temporary_file), "Config fixture cleanup failed");
        expect(runtime::read_prompt_config(temporary_file).style == runtime::PromptStyle::Xbox360,
               "Absent INI defaults to Xbox");
        *reinterpret_cast<WORD*>(pad_type.data() + 20) = 100;
        *reinterpret_cast<WORD*>(keyboard_type.data() + 20) = 99;
        void* pad_vtable[] = {reinterpret_cast<void*>(&pad_get_type)};
        void* keyboard_vtable[] = {reinterpret_cast<void*>(&keyboard_get_type)};
        alignas(4) BYTE tank[36]{}, soldier[36]{}, keyboard[36]{}, axis[36]{};
        *reinterpret_cast<void**>(tank) = pad_vtable;
        *reinterpret_cast<void**>(soldier) = pad_vtable;
        *reinterpret_cast<void**>(keyboard) = keyboard_vtable;
        *reinterpret_cast<void**>(axis) = pad_vtable;
        *reinterpret_cast<DWORD*>(tank + 20) = 15;
        *reinterpret_cast<DWORD*>(soldier + 20) = 6;
        *reinterpret_cast<DWORD*>(keyboard + 20) = 7;
        *reinterpret_cast<DWORD*>(axis + 20) = 60;
        *reinterpret_cast<DWORD*>(axis + 12) = 6;
        runtime::InputBindingPair pairs[] = {{keyboard, 0}, {tank, 0}};
        runtime::InputBindingVector bindings{pairs, pairs + 2, 0, 0};
        expect(!std::strcmp(runtime::choose_native_pad_frame(&bindings, 100, 100), "IDB_Rtrigger"),
               "Tank native frame");
        pairs[1].action = soldier;
        expect(!std::strcmp(runtime::choose_native_pad_frame(&bindings, 100, 100), "IDB_Rleft"),
               "Soldier native frame");
        pairs[1].action = axis;
        expect(!std::strcmp(runtime::choose_native_pad_frame(&bindings, 100, 100), "IDA_Axis1X"),
               "Stick native frame");
        pairs[1].action = nullptr;
        expect(!runtime::choose_native_pad_frame(&bindings, 100, 100),
               "Keyboard cannot supply pad frame");
        expect(!runtime::choose_native_pad_frame(nullptr, 100, 100), "Null vector guard");
        expect(!runtime::choose_native_pad_frame(&bindings, 0, 0),
               "Unavailable type interval guard");
        bindings.last =
            reinterpret_cast<runtime::InputBindingPair*>(reinterpret_cast<BYTE*>(pairs) + 1);
        expect(!runtime::choose_native_pad_frame(&bindings, 100, 100), "Malformed vector guard");
        expect(!runtime::pad_frame(59, 24, 3) && !runtime::pad_frame(60, 24, 3),
               "Unavailable pad IDs");

        Image image(static_cast<BYTE*>(VirtualAlloc(nullptr, runtime::address::image_size,
                                                    MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE)));
        expect(bool(image), "VirtualAlloc failed");
        expect(!runtime::has_expected_prompt_target(image.get(), runtime::address::image_size),
               "PS3 constructor guard must reject unknown code");
        constexpr BYTE ctor_prefix[] = {0x56, 0x8b, 0xf1, 0x57, 0x8b, 0x7c, 0x24,
                                        0x0c, 0xc7, 0x06, 0x88, 0xaa, 0x0a, 0x02};
        constexpr BYTE ctor_body[] = {0x8b, 0x44, 0x24, 0x10, 0x8b, 0x4c, 0x24,
                                      0x14, 0x89, 0x46, 0x0c, 0x89, 0x4e, 0x10};
        constexpr BYTE ctor_tail[] = {0x5f, 0x8b, 0xc6, 0x5e, 0xc2, 0x0c, 0x00};
        std::memcpy(image.get() + runtime::address::memory_file_ctor, ctor_prefix,
                    sizeof(ctor_prefix));
        std::memcpy(image.get() + runtime::address::memory_file_ctor + 0x24, ctor_body,
                    sizeof(ctor_body));
        std::memcpy(image.get() + runtime::address::memory_file_ctor + 0x4a, ctor_tail,
                    sizeof(ctor_tail));
        expect(runtime::has_expected_prompt_target(image.get(), runtime::address::image_size),
               "Native memory file ABI guard");
        image.get()[runtime::address::memory_file_ctor + 0x4e] ^= 1;
        expect(!runtime::has_expected_prompt_target(image.get(), runtime::address::image_size),
               "Changed calling convention accepted");
        constexpr BYTE platform_instruction[] = {0x68, 0x8c, 0x0f, 0x07, 0x02};
        std::memcpy(image.get() + runtime::address::platform_instruction, platform_instruction, 5);
        std::memcpy(image.get() + runtime::address::win32_string, "win32", 6);
        expect(runtime::is_win32_ui(image.get(), runtime::address::image_size),
               "Original PC UI platform guard");
        expect(!runtime::is_win32_ui(image.get(), 100) &&
                   !runtime::is_win32_ui(nullptr, runtime::address::image_size),
               "Truncated UI guard");
        image.get()[runtime::address::platform_instruction] ^= 1;
        expect(!runtime::is_win32_ui(image.get(), runtime::address::image_size),
               "UI instruction mismatch guard");
        image.get()[runtime::address::platform_instruction] ^= 1;
        for (unsigned i = 0; i < runtime::ui_scaling_patch_count; ++i) {
            const auto& patch = runtime::ui_scaling_patches[i];
            std::memcpy(image.get() + patch.rva, patch.expected, patch.size);
        }
        image.get()[runtime::ui_scaling_patches[5].rva] ^= 1;
        expect(runtime::applyUiScalingPatches(image.get(), runtime::address::image_size) == 5,
               "UI scaling preflight guard");
        expect(!std::memcmp(image.get() + runtime::ui_scaling_patches[0].rva,
                            runtime::ui_scaling_patches[0].expected,
                            runtime::ui_scaling_patches[0].size),
               "Scaling preflight mutated image");
        image.get()[runtime::ui_scaling_patches[5].rva] ^= 1;
        expect(runtime::applyUiScalingPatches(image.get(), 100) >= 0, "Scaling size guard");
        expect(runtime::applyUiScalingPatches(image.get(), runtime::address::image_size) == -1,
               "Scaling failed");
        for (unsigned i = 0; i < runtime::ui_scaling_patch_count; ++i) {
            const auto& patch = runtime::ui_scaling_patches[i];
            expect(!std::memcmp(image.get() + patch.rva, patch.replacement, patch.size),
                   "Scaling payload drift");
            MEMORY_BASIC_INFORMATION info{};
            expect(VirtualQuery(image.get() + patch.rva, &info, sizeof(info)) &&
                       info.Protect == PAGE_READWRITE,
                   "Page protection not restored");
        }
        expect(runtime::is_win32_ui(image.get(), runtime::address::image_size),
               "Scaling changed shared platform");
        expect(runtime::hash_rule_count() == 38, "Expected exact 38-entry hash whitelist");
        for (std::size_t i = 0; i < runtime::hash_rule_count(); ++i) {
            const auto& rule = runtime::hash_rules()[i];
            BYTE digest[20];
            std::memcpy(digest, rule.modified, 20);
            expect(runtime::apply_hash_rule(digest) == i + 1 &&
                       !std::memcmp(digest, rule.original, 20),
                   "Whitelist exact match");
            std::memcpy(digest, rule.modified, 20);
            digest[19] ^= 1;
            BYTE before[20];
            std::memcpy(before, digest, 20);
            expect(!runtime::apply_hash_rule(digest) && !std::memcmp(digest, before, 20),
                   "Whitelist accepted a near miss");
            std::memcpy(digest, rule.original, 20);
            expect(!runtime::apply_hash_rule(digest) && !std::memcmp(digest, rule.original, 20),
                   "Original hash rewritten");
        }
        std::cout << "Native pad ABI/context, PC UI guards, 14 scaling patches and exact 38-entry "
                     "whitelist passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
