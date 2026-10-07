#include <windows.h>
#include "bf3/hash.hpp"
#include "bf3/runtime/engine.hpp"
#include "bf3/runtime/log.hpp"
#include "bf3/runtime/prompt_hook.hpp"
#include "bf3/runtime/prompt_movie.hpp"
#include <MinHook.h>
#include <array>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>

namespace {
void expect(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}
bf3::Bytes read(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file)
        throw std::runtime_error("Cannot read private prompt fixture");
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}
struct MemoryFile {
    DWORD vtable, references;
    const char* name;
    const BYTE* data;
    int size, position;
    bool valid;
};
static_assert(offsetof(MemoryFile, data) == 0x0c);
using Constructor = void*(__thiscall*)(void*, const char*, const BYTE*, int);
} // namespace

int wmain(int argc, wchar_t** argv) {
    BYTE* image = nullptr;
    bool minhook_started = false;
    try {
        namespace rt = bf3::runtime;
        expect(rt::initialize_paths(GetModuleHandleW(nullptr)), "Test path initialization");
        image = static_cast<BYTE*>(VirtualAlloc(nullptr, rt::address::image_size,
                                                MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE));
        expect(image != nullptr, "ABI fixture allocation");
        auto* code = image + rt::address::memory_file_ctor;
        // Synthetic constructor with the native guarded ABI. It borrows data,
        // records all three arguments, returns this, and pops 12 stack bytes.
        // No proprietary executable bytes or engine string helper are needed.
        std::memset(code, 0x90, 0x60);
        constexpr BYTE prefix[] = {0x56, 0x8b, 0xf1, 0x57, 0x8b, 0x7c, 0x24, 0x0c,
                                   0xc7, 0x06, 0x88, 0xaa, 0x0a, 0x02, 0xeb, 0x14};
        constexpr BYTE body[] = {0x8b, 0x44, 0x24, 0x10, 0x8b, 0x4c, 0x24, 0x14, 0x89,
                                 0x46, 0x0c, 0x89, 0x4e, 0x10, 0x89, 0x7e, 0x08};
        constexpr BYTE tail[] = {0x5f, 0x8b, 0xc6, 0x5e, 0xc2, 0x0c, 0x00};
        std::memcpy(code, prefix, sizeof(prefix));
        std::memcpy(code + 0x24, body, sizeof(body));
        std::memcpy(code + 0x4a, tail, sizeof(tail));
        FlushInstructionCache(GetCurrentProcess(), code, 0x60);
        expect(rt::has_expected_prompt_target(image, rt::address::image_size), "ABI code guard");
        const auto ctor = reinterpret_cast<Constructor>(code);
        const bf3::Bytes small{1, 2, 3};
        const char name[] = "ui/assets/actionscriptlibrary.gfx";
        MemoryFile file{};
        expect(ctor(&file, name, small.data(), 3) == &file && file.name == name &&
                   file.data == small.data() && file.size == 3,
               "Original constructor ABI");
        const bf3::Bytes untouched_code(code, code + 0x60);
        expect(MH_Initialize() == MH_OK, "MinHook test initialization");
        minhook_started = true;
        expect(rt::install_ps3_prompt_hook(image), "Optional PS3 hook installation");
        expect(ctor(&file, name, small.data(), 3) == &file && file.name == name &&
                   file.data == small.data() && file.size == 3,
               "Unrelated movie was changed");
        if (argc == 2) {
            for (const auto* filename : {L"36-xbox.gfx", L"37-xbox.gfx"}) {
                auto xbox = read(std::filesystem::path(argv[1]) / filename);
                const auto before = bf3::sha256(xbox);
                const auto expected = rt::make_ps3_prompt_movie(xbox.data(), xbox.size());
                expect(!expected.bytes.empty(), "Private Xbox fixture identity");
                expect(ctor(&file, name, xbox.data(), static_cast<int>(xbox.size())) == &file &&
                           file.name == name && file.size == static_cast<int>(xbox.size()) &&
                           file.data != xbox.data() &&
                           !std::memcmp(file.data, expected.bytes.data(), expected.bytes.size()),
                       "Hook did not supply PS3 copy with original ABI");
                const auto* stable = file.data;
                expect(before == bf3::sha256(xbox), "Hook mutated engine's Xbox source");
                ctor(&file, name, xbox.data(), static_cast<int>(xbox.size()));
                expect(file.data == stable, "Overlay lifetime/cache changed");
                xbox.back() ^= 1; // Same size/pointer, different library: must pass through.
                ctor(&file, name, xbox.data(), static_cast<int>(xbox.size()));
                expect(file.data == xbox.data(), "Pointer reuse bypassed exact source guard");
                // Constructor only borrows this invalid address; the guarded
                // overlay must release its lock and fall back on an access fault.
                const auto* unreadable = reinterpret_cast<const BYTE*>(1);
                ctor(&file, name, unreadable, static_cast<int>(xbox.size()));
                expect(file.data == unreadable, "Unreadable source did not pass through");
                xbox.back() ^= 1;
                ctor(&file, name, xbox.data(), static_cast<int>(xbox.size()));
                expect(file.data == stable, "Access fault left the overlay locked");
            }
        }
        expect(MH_DisableHook(code) == MH_OK && MH_RemoveHook(code) == MH_OK &&
                   MH_Uninitialize() == MH_OK,
               "Hook removal");
        minhook_started = false;
        expect(!std::memcmp(code, untouched_code.data(), untouched_code.size()),
               "Hook teardown did not restore code");
        VirtualFree(image, 0, MEM_RELEASE);
        std::cout << "Native constructor ABI, exact movie guards, immutable PS3 copies, pointer "
                     "reuse, lifetime, access-fault fallback and hook teardown passed.\n";
        return 0;
    } catch (const std::exception& error) {
        if (minhook_started)
            MH_Uninitialize();
        if (image)
            VirtualFree(image, 0, MEM_RELEASE);
        std::cerr << error.what() << '\n';
        return 1;
    }
}
