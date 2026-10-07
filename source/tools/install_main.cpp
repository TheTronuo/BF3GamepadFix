#include <windows.h>
#include "bf3/hash.hpp"
#include "bf3/install/transaction.hpp"
#include "bf3/patch/release.hpp"
#include <array>
#include <fstream>
#include <iostream>
#include <sstream>
#include <tlhelp32.h>

namespace fs = std::filesystem;
namespace patch = bf3::patch;
namespace setup = bf3::install;
using bf3::Bytes;

namespace {
constexpr char original_dll_sha256[] = BF3_ORIGINAL_DLL_SHA256;
constexpr char release_dll_sha256[] = BF3_RELEASE_DLL_SHA256;
constexpr wchar_t dll_name[] = L"Engine.BuildInfo_Win32_Retail_dll.dll";

Bytes read_bytes(const fs::path& path, std::uint64_t offset = 0,
                 std::size_t length = static_cast<std::size_t>(-1)) {
    const auto size = fs::file_size(path);
    if (offset > size)
        throw std::runtime_error("Read offset exceeds file size");
    if (length == static_cast<std::size_t>(-1))
        length = static_cast<std::size_t>(size - offset);
    if (length > size - offset || length > 16 * 1024 * 1024)
        throw std::runtime_error("Invalid installer input size");
    Bytes bytes(length);
    std::ifstream input(path, std::ios::binary);
    input.seekg(static_cast<std::streamoff>(offset));
    if (!input || !input.read(reinterpret_cast<char*>(bytes.data()),
                              static_cast<std::streamsize>(bytes.size())))
        throw std::runtime_error("Cannot read input: " + path.string());
    return bytes;
}

Bytes checked_file(const fs::path& path, const std::string& expected) {
    auto bytes = read_bytes(path);
    if (bf3::sha256(bytes) != expected)
        throw std::runtime_error("File identity mismatch: " + path.string());
    return bytes;
}

fs::path executable_directory() {
    std::array<wchar_t, 32768> filename{};
    const auto length =
        GetModuleFileNameW(nullptr, filename.data(), static_cast<DWORD>(filename.size()));
    if (!length || length >= filename.size())
        throw std::runtime_error("Cannot locate installer directory");
    return fs::path(filename.data()).parent_path();
}

void require_closed_game() {
    const auto snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
        throw std::runtime_error("Cannot check whether Battlefield 3 is running");
    PROCESSENTRY32W process{};
    process.dwSize = sizeof(process);
    bool running = false;
    if (Process32FirstW(snapshot, &process)) {
        do {
            running = running || _wcsicmp(process.szExeFile, L"bf3.exe") == 0;
        } while (Process32NextW(snapshot, &process));
    }
    CloseHandle(snapshot);
    if (running)
        throw std::runtime_error("Close Battlefield 3 before installing or removing the mod");
}

std::string state_value(const fs::path& state, const wchar_t* key) {
    std::array<wchar_t, 128> value{};
    GetPrivateProfileStringW(L"Install", key, L"", value.data(), static_cast<DWORD>(value.size()),
                             state.c_str());
    std::string text;
    for (const auto* p = value.data(); *p; ++p) {
        if (*p > 127)
            throw std::runtime_error("Invalid installation state");
        text.push_back(static_cast<char>(*p));
    }
    return text;
}

void add_file(std::vector<setup::FileChange>& files, const fs::path& path, Bytes after) {
    const bool existed = fs::exists(path);
    files.push_back({path, existed, existed ? read_bytes(path) : Bytes{}, std::move(after)});
}

void verify_metadata(const fs::path& game) {
    for (std::size_t i = 0; i < patch::release_metadata_count(); ++i) {
        const auto& spec = patch::release_metadata()[i];
        if (bf3::file_sha256(setup::contained_path(game, spec.path)) != spec.sha256)
            throw std::runtime_error(std::string("Unsupported game metadata: ") + spec.path);
    }
}

std::vector<setup::ArchiveChange> archive_plan(const fs::path& game, const fs::path& package,
                                               const fs::path& state, bool removing) {
    std::vector<setup::ArchiveChange> archives;
    for (std::size_t i = 0; i < patch::release_archive_count(); ++i) {
        const auto& spec = patch::release_archives()[i];
        const auto path = setup::contained_path(game, spec.path);
        const auto current = bf3::file_sha256(path);
        if (fs::file_size(path) != spec.size ||
            (current != spec.original_sha256 && current != spec.patched_sha256))
            throw std::runtime_error(std::string("Unsupported or modified archive: ") + spec.path);
        std::string target = spec.patched_sha256;
        if (removing) {
            target = state_value(state, (L"Archive" + std::to_wstring(i)).c_str());
            if (target != spec.original_sha256 && target != spec.patched_sha256)
                throw std::runtime_error("Previous archive identity is missing from the state");
        }
        setup::ArchiveChange archive{path, spec.size, current, target, {}};
        const bool current_original = current == spec.original_sha256;
        const bool target_original = target == spec.original_sha256;
        for (std::size_t resource = 0; resource < patch::release_resource_count(); ++resource) {
            const auto& range = patch::release_resources()[resource];
            if (std::string(range.archive) != spec.path)
                continue;
            auto before = read_bytes(path, range.offset, range.stored_size);
            if (bf3::sha256(before) !=
                (current_original ? range.original_sha256 : range.patched_sha256))
                throw std::runtime_error("Resource identity mismatch before installation");
            const auto delta =
                read_bytes(package / "patches" / (std::string(range.patched_file) + ".xor"));
            if (delta.size() != before.size())
                throw std::runtime_error("Damaged XOR patch size");
            auto other = before;
            for (std::size_t byte = 0; byte < before.size(); ++byte)
                other[byte] ^= delta[byte];
            if (bf3::sha256(other) !=
                (current_original ? range.patched_sha256 : range.original_sha256))
                throw std::runtime_error("Damaged XOR patch data");
            auto after = target_original == current_original ? before : std::move(other);
            archive.ranges.push_back({range.offset, std::move(before), std::move(after)});
        }
        archives.push_back(std::move(archive));
        std::cout << "Archive verified: " << spec.path << '\n';
    }
    return archives;
}

Bytes original_dll(const fs::path& game) {
    for (const auto& relative :
         {fs::path(L"ControllerMod/loader/original-buildinfo.dll"),
          fs::path(L"ori_Engine.BuildInfo_Win32_Retail_dll.dll"), fs::path(dll_name)}) {
        const auto candidate = setup::contained_path(game, relative);
        if (fs::exists(candidate) && bf3::file_sha256(candidate) == original_dll_sha256)
            return checked_file(candidate, original_dll_sha256);
    }
    throw std::runtime_error("Supported original BuildInfo DLL not found; verify game files first");
}
} // namespace

void bf3_require_closed_game() { require_closed_game(); }

int bf3_install_main(int argc, wchar_t** argv, const fs::path& embedded_package) {
    bool interactive = argc == 1;
    int result = 0;
    try {
        const auto directory = executable_directory();
        std::wstring action;
        if (interactive) {
            std::cout << "BF3GamepadFix 0.22.4 pre-release\n"
                         "1 Install   2 Restore previous installation   3 Status\n> ";
            std::string choice;
            std::getline(std::cin, choice);
            action = choice == "1" ? L"install" : choice == "2" ? L"remove" : L"status";
        } else if (argc == 2 || argc == 3) {
            action = argv[1];
        }
        if (action != L"install" && action != L"remove" && action != L"status")
            throw std::runtime_error(
                "Usage: BF3GamepadFix.exe install|remove|status [GAME_DIRECTORY]");
        const auto game = fs::weakly_canonical(argc == 3 ? fs::path(argv[2]) : directory);
        const auto package = embedded_package.empty() ? directory / "BF3GamepadFix" : embedded_package;
        const auto state = setup::contained_path(game, "ControllerMod/BF3GamepadFix/state.ini");
        const auto previous =
            setup::contained_path(game, "ControllerMod/BF3GamepadFix/previous-loader.dll");
        const auto live = setup::contained_path(game, dll_name);
        if (!fs::is_regular_file(setup::contained_path(game, "bf3.exe")))
            throw std::runtime_error("Choose the Battlefield 3 directory containing bf3.exe");
        require_closed_game();
        verify_metadata(game);
        const bool active = state_value(state, L"Active") == "1";
        if (action == L"remove" && !active) {
            std::cout << "No active BF3GamepadFix installation recorded; nothing changed.\n";
        } else {
            auto archives = archive_plan(game, package, state, action == L"remove");
            const auto live_bytes = read_bytes(live);
            const auto live_hash = bf3::sha256(live_bytes);
            if (action == L"status") {
                std::cout << "BF3GamepadFix installation record: " << (active ? "active" : "absent")
                          << "; release DLL: " << (live_hash == release_dll_sha256 ? "yes" : "no")
                          << ". No files changed.\n";
            } else if (action == L"install" && active) {
                if (live_hash != release_dll_sha256)
                    throw std::runtime_error("Installed loader differs from the recorded release");
                for (std::size_t i = 0; i < archives.size(); ++i)
                    if (archives[i].before_sha256 != patch::release_archives()[i].patched_sha256)
                        throw std::runtime_error("Recorded installation has changed archives");
                checked_file(previous, state_value(state, L"PreviousLoaderSha256"));
                std::cout << "BF3GamepadFix is already installed; nothing changed.\n";
            } else {
                std::vector<setup::FileChange> files;
                if (action == L"install") {
                    const auto original = original_dll(game);
                    const auto backup =
                        setup::contained_path(game, "ControllerMod/loader/original-buildinfo.dll");
                    if (fs::exists(backup))
                        checked_file(backup, original_dll_sha256);
                    else
                        add_file(files, backup, original);
                    const auto replacement =
                        checked_file(package / "runtime" / dll_name, release_dll_sha256);
                    add_file(files, previous, live_bytes);
                    const auto config = setup::contained_path(game, "BF3Controller.ini");
                    if (!fs::exists(config))
                        add_file(files, config, read_bytes(package / "BF3Controller.ini"));
                    add_file(files, live, replacement);
                    std::ostringstream record;
                    record << "[Install]\nActive=1\nPreviousLoaderSha256=" << live_hash << '\n';
                    for (std::size_t i = 0; i < archives.size(); ++i)
                        record << "Archive" << i << '=' << archives[i].before_sha256 << '\n';
                    const auto text = record.str();
                    add_file(files, state, Bytes(text.begin(), text.end()));
                } else {
                    if (live_hash != release_dll_sha256)
                        throw std::runtime_error(
                            "Loader changed after installation; restore was stopped");
                    add_file(files, live,
                             checked_file(previous, state_value(state, L"PreviousLoaderSha256")));
                    const std::string text = "[Install]\nActive=0\n";
                    add_file(files, state, Bytes(text.begin(), text.end()));
                }
                setup::apply_transaction(archives, files);
                std::cout << (action == L"install"
                                  ? "Installed and verified. Restart BF3.\n"
                                  : "Previous archives and loader restored and verified.\n");
            }
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        result = 1;
    }
    if (interactive) {
        std::cout << "Press Enter to close.\n";
        std::string ignored;
        std::getline(std::cin, ignored);
    }
    return result;
}

#ifndef BF3_GUI
int wmain(int argc, wchar_t** argv) {
    return bf3_install_main(argc, argv, {});
}
#endif
