#pragma once

#include <windows.h>

namespace bf3::runtime {
struct UiScalingPatch {
    DWORD rva;
    unsigned size;
    const BYTE* expected;
    const BYTE* replacement;
    const char* name;
};
extern const UiScalingPatch ui_scaling_patches[14];
extern const unsigned ui_scaling_patch_count;

// -1: applied; >=0: rejected range; -2: write failure, rollback attempted.
int applyUiScalingPatches(BYTE* base, DWORD image_size);
} // namespace bf3::runtime
