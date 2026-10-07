#include "bf3/runtime/ui_scaling.hpp"
#include <cstring>

namespace bf3::runtime {
#include "ui_scaling_data.inc"

static bool write_ui_bytes(BYTE* address, const BYTE* data, unsigned size) {
    DWORD oldProtection;
    if (!VirtualProtect(address, size, PAGE_EXECUTE_READWRITE, &oldProtection))
        return false;
    memcpy(address, data, size);
    BOOL flushed = FlushInstructionCache(GetCurrentProcess(), address, size);
    DWORD unused;
    BOOL restored = VirtualProtect(address, size, oldProtection, &unused);
    return flushed && restored;
}
// -1: applied; >=0: rejected range; -2: OS write failure, original bytes restored.
int applyUiScalingPatches(BYTE* base, DWORD imageSize) {
    for (unsigned i = 0; i < ui_scaling_patch_count; ++i) {
        const UiScalingPatch& p = ui_scaling_patches[i];
        if (p.rva >= imageSize || p.size > imageSize - p.rva ||
            memcmp(base + p.rva, p.expected, p.size) != 0)
            return (int)i;
    }
    for (unsigned i = 0; i < ui_scaling_patch_count; ++i) {
        const UiScalingPatch& p = ui_scaling_patches[i];
        if (!write_ui_bytes(base + p.rva, p.replacement, p.size)) {
            for (unsigned j = 0; j <= i; ++j) {
                const UiScalingPatch& undo = ui_scaling_patches[j];
                write_ui_bytes(base + undo.rva, undo.expected, undo.size);
            }
            return -2;
        }
    }
    return -1;
}

} // namespace bf3::runtime
