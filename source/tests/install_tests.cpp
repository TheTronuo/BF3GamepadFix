#include <windows.h>
#include "bf3/hash.hpp"
#include "bf3/install/transaction.hpp"
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;
namespace setup = bf3::install;
using bf3::Bytes;

namespace {
void expect(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}
void write(const fs::path& path, const Bytes& bytes) {
    std::ofstream output(path, std::ios::binary);
    output.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
    if (!output)
        throw std::runtime_error("Fixture write failed");
}
template <class Function> void expect_rejection(Function action) {
    bool rejected = false;
    try {
        action();
    } catch (const std::exception&) {
        rejected = true;
    }
    expect(rejected, "Expected installer rejection");
}
} // namespace

int main() {
    const auto root = fs::current_path() /
                      ("bf3gamepadfix-installer-test-" + std::to_string(GetCurrentProcessId()));
    try {
        if (!fs::create_directory(root))
            throw std::runtime_error("Test directory already exists");
        const auto archive = root / "archive.bin";
        const auto dll = root / "loader.bin";
        const auto state = root / "state.ini";
        const Bytes original(512, 42), old_dll{1, 2, 3}, new_dll{4, 5, 6};
        auto patched = original;
        patched[100] = 99;
        patched[101] = 88;
        write(archive, original);
        write(dll, old_dll);
        const setup::ArchiveChange change{archive,
                                          original.size(),
                                          bf3::sha256(original),
                                          bf3::sha256(patched),
                                          {{100, {42, 42}, {99, 88}}}};
        const std::vector<setup::FileChange> files{{dll, true, old_dll, new_dll},
                                                   {state, false, {}, {7}}};
        expect_rejection([&] {
            setup::apply_transaction({change}, files,
                                     [] { throw std::runtime_error("Injected I/O failure"); });
        });
        expect(bf3::file_sha256(archive) == bf3::sha256(original),
               "Failed write must restore archive");
        expect(bf3::file_sha256(dll) == bf3::sha256(old_dll) && !fs::exists(state),
               "Failure must preserve prior files");
        auto damaged = original;
        damaged.back() ^= 1;
        write(archive, damaged);
        expect_rejection([&] { setup::apply_transaction({change}, files); });
        expect(bf3::file_sha256(archive) == bf3::sha256(damaged),
               "Unsupported archive must remain untouched");
        write(archive, original);
        auto blocked_temporary = dll;
        blocked_temporary += L".bf3gamepadfix-tmp";
        write(blocked_temporary, {10});
        expect_rejection([&] { setup::apply_transaction({change}, files); });
        expect(bf3::file_sha256(archive) == bf3::sha256(original) &&
                   bf3::file_sha256(dll) == bf3::sha256(old_dll) && !fs::exists(state),
               "DLL replacement failure must restore mutated archives and preserve prior files");
        fs::remove(blocked_temporary);
        auto bad_range = change;
        bad_range.ranges[0].offset = 512;
        expect_rejection([&] { setup::apply_transaction({bad_range}, files); });
        expect(bf3::file_sha256(archive) == bf3::sha256(original),
               "Out-of-bounds patch must not write");
        auto overlapping = change;
        overlapping.ranges.push_back(change.ranges[0]);
        expect_rejection([&] { setup::apply_transaction({overlapping}, files); });
        expect_rejection([&] { setup::contained_path(root, "../outside.bin"); });
        expect(setup::contained_path(root, "archive.bin") == archive,
               "Contained file must resolve");
        setup::apply_transaction({change}, files);
        expect(bf3::file_sha256(archive) == bf3::sha256(patched), "Install archive parity");
        expect(bf3::file_sha256(dll) == bf3::sha256(new_dll) && fs::exists(state),
               "Install DLL and state");
        const setup::ArchiveChange restore{archive,
                                           patched.size(),
                                           bf3::sha256(patched),
                                           bf3::sha256(original),
                                           {{100, {99, 88}, {42, 42}}}};
        setup::apply_transaction({restore}, {{dll, true, new_dll, old_dll}});
        expect(bf3::file_sha256(archive) == bf3::sha256(original) &&
                   bf3::file_sha256(dll) == bf3::sha256(old_dll),
               "Exact restore parity");
        fs::remove_all(root);
        std::cout
            << "Installer transaction, rollback, archive guards and path containment passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
