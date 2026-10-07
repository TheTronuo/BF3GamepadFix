#include <windows.h>
#include "bf3/hash.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;
using bf3::Bytes;

namespace {
void expect(bool result, const char* message) {
    if (!result)
        throw std::runtime_error(message);
}
void write(const fs::path& path, const Bytes& bytes) {
    fs::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    output.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
    if (!output)
        throw std::runtime_error("Fixture write failed");
}
void run(const fs::path& executable, const fs::path& game, const wchar_t* action,
         bool success = true) {
    auto command = L"\"" + executable.wstring() + L"\" " + action + L" \"" + game.wstring() + L"\"";
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, FALSE,
                        CREATE_NO_WINDOW, nullptr, game.c_str(), &startup, &process))
        throw std::runtime_error("Cannot start CLI test fixture");
    CloseHandle(process.hThread);
    if (WaitForSingleObject(process.hProcess, 30000) != WAIT_OBJECT_0) {
        TerminateProcess(process.hProcess, 1);
        CloseHandle(process.hProcess);
        throw std::runtime_error("CLI fixture timed out");
    }
    DWORD code = 1;
    GetExitCodeProcess(process.hProcess, &code);
    CloseHandle(process.hProcess);
    expect((code == 0) == success, "Unexpected installer CLI exit code");
}
} // namespace

int wmain(int argc, wchar_t** argv) {
    if (argc != 2)
        return 2;
    const auto root =
        fs::current_path() / ("bf3gamepadfix-cli-test-" + std::to_string(GetCurrentProcessId()));
    try {
        if (!fs::create_directory(root))
            throw std::runtime_error("CLI test directory already exists");
        const auto game = root / "game";
        const auto package = root / "installer" / "BF3GamepadFix";
        const auto executable = root / "installer" / "fixture-installer.exe";
        fs::create_directories(executable.parent_path());
        fs::copy_file(argv[1], executable);
        const Bytes original(512, 42), original_dll{1, 2, 3}, release_dll{4, 5, 6};
        const Bytes prior_ui_fix{10, 11, 12}, metadata{8, 9};
        const Bytes xor_patch{static_cast<std::uint8_t>(42 ^ 99),
                              static_cast<std::uint8_t>(42 ^ 88)};
        auto patched = original;
        patched[100] = 99;
        patched[101] = 88;
        const std::string config = "[Controller]\nPromptStyle=PS3\n";
        const Bytes config_bytes(config.begin(), config.end());
        const auto live = game / "Engine.BuildInfo_Win32_Retail_dll.dll";
        const auto archive = game / "Data/test.cas";
        write(game / "bf3.exe", {0}); // Never executed: synthetic game marker only.
        write(archive, original);
        write(game / "Data/test.toc", metadata);
        write(live, original_dll);
        write(game / "BF3Controller.ini", config_bytes);
        write(package / "runtime" / live.filename(), release_dll);
        write(package / "patches/fixture.patched.xor", xor_patch);
        write(package / "BF3Controller.ini", {0});
        run(executable, game, L"install");
        expect(bf3::file_sha256(archive) == bf3::sha256(patched) &&
                   bf3::file_sha256(live) == bf3::sha256(release_dll),
               "Fresh CLI installation parity");
        expect(bf3::file_sha256(game / "BF3Controller.ini") == bf3::sha256(config_bytes),
               "Existing configuration must be preserved");
        run(executable, game, L"status");
        run(executable, game, L"install");
        run(executable, game, L"remove");
        run(executable, game, L"remove");
        expect(bf3::file_sha256(archive) == bf3::sha256(original) &&
                   bf3::file_sha256(live) == bf3::sha256(original_dll),
               "Fresh CLI exact restore");
        write(live, prior_ui_fix);
        write(game / "ori_Engine.BuildInfo_Win32_Retail_dll.dll", original_dll);
        fs::remove(game / "ControllerMod/loader/original-buildinfo.dll");
        run(executable, game, L"install");
        run(executable, game, L"remove");
        expect(bf3::file_sha256(live) == bf3::sha256(prior_ui_fix) &&
                   bf3::file_sha256(archive) == bf3::sha256(original),
               "Existing UI fix loader must be restored");
        auto bad_archive = original;
        bad_archive.back() ^= 1;
        write(archive, bad_archive);
        run(executable, game, L"install", false);
        expect(bf3::file_sha256(archive) == bf3::sha256(bad_archive) &&
                   bf3::file_sha256(live) == bf3::sha256(prior_ui_fix),
               "Unsupported archive must remain unchanged");
        write(archive, original);
        write(package / "patches/fixture.patched.xor", {0, 0});
        run(executable, game, L"install", false);
        expect(bf3::file_sha256(archive) == bf3::sha256(original) &&
                   bf3::file_sha256(live) == bf3::sha256(prior_ui_fix),
               "Damaged package must remain unchanged");
        write(package / "patches/fixture.patched.xor", xor_patch);
        write(game / "Data/test.toc", {0});
        run(executable, game, L"install", false);
        expect(bf3::file_sha256(archive) == bf3::sha256(original) &&
                   bf3::file_sha256(live) == bf3::sha256(prior_ui_fix),
               "Unsupported metadata must remain unchanged");
        fs::remove_all(root);
        std::cout << "CLI fresh install, repeated install, status, exact restore, UI fix "
                     "migration, config preservation and rejection checks passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
