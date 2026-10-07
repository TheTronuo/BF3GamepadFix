#include "bf3/runtime/config.hpp"
#include "bf3/runtime/engine.hpp"
#include "bf3/runtime/hooks.hpp"
#include "bf3/runtime/log.hpp"
#include "bf3/runtime/prompt_hook.hpp"
#include "bf3/runtime/ui_scaling.hpp"

namespace bf3::runtime {
void initialize_runtime() noexcept {
    static LONG initialized = 0;
    if (InterlockedCompareExchange(&initialized, 1, 0) != 0)
        return;
    auto* image = reinterpret_cast<BYTE*>(GetModuleHandleW(nullptr));
    log_line(
        "Controller Mod 0.22.3 C++: Win32 menus; native Xbox actions; configurable prompt artwork; "
        "UI scaling");
    if (!is_supported_executable(image)) {
        log_line("Unsupported executable; runtime changes skipped");
        return;
    }
    if (applyUiScalingPatches(image, address::image_size) != -1) {
        log_line("UI scaling guard/write failed; Xbox hooks skipped");
        return;
    }
    log_line("UI scaling: all 14 original guarded patches applied");
    if (install_hooks(image)) {
        log_line("Xbox prompts active: shared UI Platform remains win32; four local xenon widgets; "
                 "native PadInputActionData IDs");
        const auto config = read_prompt_config(config_path());
        if (!config.valid)
            log_line("Invalid PromptStyle; safe default Xbox360 selected");
        if (config.style == PromptStyle::PS3) {
            if (install_ps3_prompt_hook(image))
                log_line("PromptStyle=PS3: optional memory overlay active; Xbox input unchanged");
        } else {
            log_line("PromptStyle=Xbox360: original Xbox path; no presentation hook installed");
        }
    }
}
} // namespace bf3::runtime
