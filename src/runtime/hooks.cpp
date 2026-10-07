#include "bf3/runtime/hooks.hpp"
#include "bf3/runtime/engine.hpp"
#include "bf3/runtime/hash_rules.hpp"
#include "bf3/runtime/input_frames.hpp"
#include "bf3/runtime/log.hpp"
#include <MinHook.h>
#include <cstring>

namespace bf3::runtime {
namespace {
BYTE* game_base = nullptr;
using ShaFinal = int(__cdecl*)(unsigned char*, void*);
using BindingText = void*(__cdecl*)(void*, const InputBindingVector*);
using StringCtor = void(__thiscall*)(void*, const char*, const char*);
ShaFinal real_sha_final = nullptr;
BindingText real_binding_text = nullptr;

int __cdecl sha_final_hook(unsigned char* output, void* context) {
    const auto result = real_sha_final(output, context);
    if (result == 1 && output)
        apply_hash_rule(output);
    return result;
}

const char* guarded_frame(const InputBindingVector* bindings) noexcept {
    __try {
        return choose_native_pad_frame(
            bindings, *reinterpret_cast<WORD*>(game_base + address::pad_type_first),
            *reinterpret_cast<WORD*>(game_base + address::pad_type_last));
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
}

void* __cdecl binding_text_hook(void* output, const InputBindingVector* bindings) {
    const auto* frame = guarded_frame(bindings);
    if (!frame || !output)
        return real_binding_text(output, bindings);
    // Use the same engine String constructor as 0x0090C669. The active action
    // map supplies the GFx frame; no keyboard-label or localization heuristics.
    std::memset(output, 0, 16);
    reinterpret_cast<StringCtor>(game_base + address::string_ctor)(output, frame,
                                                                   frame + std::strlen(frame));
    return output;
}
} // namespace

bool install_hooks(BYTE* image) noexcept {
    if (!has_expected_hook_targets(image, address::image_size)) {
        log_line("Xbox code/data guard failed; hooks skipped");
        return false;
    }
    game_base = image;
    if (MH_Initialize() != MH_OK) {
        log_line("MinHook initialization failed");
        return false;
    }
    auto* sha = image + address::sha_final;
    auto* text = image + address::binding_text;
    if (MH_CreateHook(sha, reinterpret_cast<LPVOID>(&sha_final_hook),
                      reinterpret_cast<LPVOID*>(&real_sha_final)) != MH_OK ||
        MH_CreateHook(text, reinterpret_cast<LPVOID>(&binding_text_hook),
                      reinterpret_cast<LPVOID*>(&real_binding_text)) != MH_OK) {
        MH_RemoveHook(sha);
        MH_RemoveHook(text);
        MH_Uninitialize();
        log_line("Hook creation failed; hooks skipped");
        return false;
    }
    if (MH_QueueEnableHook(sha) != MH_OK || MH_QueueEnableHook(text) != MH_OK ||
        MH_ApplyQueued() != MH_OK) {
        MH_DisableHook(sha);
        MH_DisableHook(text);
        MH_RemoveHook(sha);
        MH_RemoveHook(text);
        MH_Uninitialize();
        log_line("Hook enable failed; hooks skipped");
        return false;
    }
    return true;
}
} // namespace bf3::runtime
