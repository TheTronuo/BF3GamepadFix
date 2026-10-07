#include "bf3/runtime/log.hpp"
#include <cstring>
#include <cwchar>

namespace bf3::runtime {
namespace {
wchar_t original_path[MAX_PATH]{};
wchar_t log_path[MAX_PATH]{};
wchar_t settings_path[MAX_PATH]{};
SRWLOCK log_lock = SRWLOCK_INIT;
} // namespace

bool initialize_paths(HINSTANCE module) noexcept {
    wchar_t directory[MAX_PATH]{};
    const auto length = GetModuleFileNameW(module, directory, MAX_PATH);
    if (!length || length >= MAX_PATH)
        return false;
    auto* separator = std::wcsrchr(directory, L'\\');
    if (!separator)
        return false;
    separator[1] = 0;
    return swprintf_s(original_path, L"%sControllerMod\\loader\\original-buildinfo.dll",
                      directory) >= 0 &&
           swprintf_s(log_path, L"%sControllerMod\\loader-diagnostic.log", directory) >= 0 &&
           swprintf_s(settings_path, L"%sBF3Controller.ini", directory) >= 0;
}

const wchar_t* original_dll_path() noexcept {
    return original_path;
}

const wchar_t* config_path() noexcept {
    return settings_path;
}

void log_line(const char* message) noexcept {
    AcquireSRWLockExclusive(&log_lock);
    const auto file = CreateFileW(log_path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                  nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        WriteFile(file, message, static_cast<DWORD>(std::strlen(message)), &written, nullptr);
        WriteFile(file, "\r\n", 2, &written, nullptr);
        CloseHandle(file);
    }
    ReleaseSRWLockExclusive(&log_lock);
}
} // namespace bf3::runtime
