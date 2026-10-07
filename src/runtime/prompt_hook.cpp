#include "bf3/runtime/prompt_hook.hpp"
#include "bf3/runtime/engine.hpp"
#include "bf3/runtime/log.hpp"
#include "bf3/runtime/prompt_movie.hpp"
#include <MinHook.h>
#include <array>
#include <cstring>

namespace bf3::runtime {
namespace {
using MemoryFileCtor = void*(__thiscall*)(void*, const char*, const BYTE*, int);
MemoryFileCtor real_memory_file_ctor = nullptr;
SRWLOCK movie_lock = SRWLOCK_INIT;
// The engine borrows the data pointer. Keep these two immutable copies alive
// until process exit; never free them when a temporary file wrapper is closed.
std::array<Bytes, 2> ps3_movies;

const BYTE* select_ps3_movie(const BYTE* data, int size) noexcept {
    if (size != 726805 && size != 662290)
        return data;
    const BYTE* result = data;
    try {
        // Check the entire source each time: an allocator may reuse a pointer.
        auto changed = make_ps3_prompt_movie(data, static_cast<std::size_t>(size));
        if (!changed.bytes.empty()) {
            auto& cached = ps3_movies[size == 726805 ? 0 : 1];
            if (cached.empty()) {
                cached = std::move(changed.bytes);
                log_line(
                    "PS3 prompt movie selected; local icons only; Xbox QTE dictionary retained");
            }
            result = cached.data();
        }
    } catch (...) {
        log_line("PS3 prompt movie guard/allocation failed; original Xbox movie retained");
    }
    return result;
}

const BYTE* guarded_ps3_movie(const BYTE* data, int size) noexcept {
    if (size != 726805 && size != 662290)
        return data;
    const BYTE* result = data;
    AcquireSRWLockExclusive(&movie_lock);
    __try {
        __try {
            result = select_ps3_movie(data, size);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            log_line("PS3 source buffer unreadable; original movie retained");
        }
    } __finally {
        ReleaseSRWLockExclusive(&movie_lock);
    }
    return result;
}

void* __fastcall memory_file_ctor_hook(void* self, void*, const char* name, const BYTE* data,
                                       int size) {
    return real_memory_file_ctor(self, name, guarded_ps3_movie(data, size), size);
}
} // namespace

bool install_ps3_prompt_hook(BYTE* image) noexcept {
    if (!has_expected_prompt_target(image, address::image_size)) {
        log_line("PS3 memory-file constructor guard failed; Xbox presentation retained");
        return false;
    }
    auto* target = image + address::memory_file_ctor;
    if (MH_CreateHook(target, reinterpret_cast<LPVOID>(&memory_file_ctor_hook),
                      reinterpret_cast<LPVOID*>(&real_memory_file_ctor)) != MH_OK) {
        log_line("PS3 presentation hook creation failed; Xbox hooks retained");
        return false;
    }
    if (MH_EnableHook(target) != MH_OK) {
        MH_RemoveHook(target);
        log_line("PS3 presentation hook enable failed; Xbox hooks retained");
        return false;
    }
    return true;
}
} // namespace bf3::runtime
