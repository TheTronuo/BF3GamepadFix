#include "bf3/runtime/hooks.hpp"
#include "bf3/runtime/log.hpp"

extern "C" {
FARPROC original_build_info = nullptr;
void __cdecl initialize_controller() {
    bf3::runtime::initialize_runtime();
}

// getBuildInfo's ABI is forwarded verbatim, with all entry registers/flags
// restored. Initialization is deliberately outside DllMain's loader lock.
__declspec(naked) void __cdecl build_info_wrapper() {
    __asm {
        pushfd
        pushad
        call initialize_controller
        popad
        popfd
        jmp dword ptr [original_build_info]
    }
}
}

BOOL WINAPI DllMain(HINSTANCE self, DWORD reason, LPVOID) {
    if (reason != DLL_PROCESS_ATTACH)
        return TRUE;
    DisableThreadLibraryCalls(self);
    if (!bf3::runtime::initialize_paths(self))
        return FALSE;
    const auto original = LoadLibraryW(bf3::runtime::original_dll_path());
    if (!original)
        return FALSE;
    original_build_info = GetProcAddress(original, "getBuildInfo");
    return original_build_info != nullptr;
}
