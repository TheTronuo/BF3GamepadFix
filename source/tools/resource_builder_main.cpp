#include "bf3/patch/resource_builder.hpp"
#include <iostream>
#include <system_error>

namespace fs = std::filesystem;
namespace patch = bf3::patch;

namespace {
bf3::Bytes text_bytes(const std::string& text) {
    return {text.begin(), text.end()};
}

// Only sibling directories under the explicitly requested output are created.
// A completed output is published with a rename after all 38 guards pass.
class OutputTransaction {
  public:
    explicit OutputTransaction(fs::path destination)
        : destination_(fs::absolute(std::move(destination))) {
        if (destination_.filename().empty() || fs::exists(destination_)) {
            throw std::runtime_error("Output must be a new directory");
        }
        staging_ = destination_;
        staging_ += ".building";
        fs::create_directories(destination_.parent_path());
        if (!fs::create_directory(staging_))
            throw std::runtime_error("Staging directory already exists");
    }
    ~OutputTransaction() {
        if (!committed_) {
            std::error_code ignored;
            fs::remove_all(staging_, ignored);
        }
    }
    const fs::path& path() const noexcept { return staging_; }
    void commit() {
        fs::rename(staging_, destination_);
        committed_ = true;
    }

  private:
    fs::path destination_, staging_;
    bool committed_ = false;
};
} // namespace

int wmain(int argc, wchar_t** argv) {
    try {
        if (argc == 3 && std::wstring(argv[1]) == L"--emit-hash-rules") {
            patch::write_bytes(fs::path(argv[2]), text_bytes(patch::generate_hash_rules()));
            return 0;
        }
        if (argc != 5 ||
            (std::wstring(argv[1]) != L"--game" && std::wstring(argv[1]) != L"--originals") ||
            std::wstring(argv[3]) != L"--output") {
            std::cout
                << "bf3-resource-builder (--game GAME | --originals STAGE) --output NEW_DIRECTORY\n"
                   "bf3-resource-builder --emit-hash-rules OUTPUT.inc\n";
            return 2;
        }
        const auto source = fs::absolute(argv[2]);
        const auto output = fs::absolute(argv[4]);
        const bool from_game = std::wstring(argv[1]) == L"--game";
        // Do not permit build products to be written inside either input tree.
        const auto canonical_source = fs::weakly_canonical(source);
        const auto canonical_output = fs::weakly_canonical(output);
        const auto relative = canonical_output.lexically_relative(canonical_source);
        if (!relative.empty() && *relative.begin() != L"..") {
            throw std::runtime_error("Output must be outside the source directory");
        }
        if (from_game)
            patch::verify_original_game(source);
        OutputTransaction transaction(output);
        for (std::size_t i = 0; i < patch::release_resource_count(); ++i) {
            const auto& spec = patch::release_resources()[i];
            const auto original =
                patch::read_resource(source / (from_game ? spec.archive : spec.original_file),
                                     from_game ? spec.offset : 0, spec.stored_size);
            const auto built = patch::rebuild_resource(spec, original);
            patch::write_bytes(transaction.path() / spec.original_file, original);
            patch::write_bytes(transaction.path() / spec.patched_file, built.bytes);
            std::cout << '[' << i + 1 << '/' << patch::release_resource_count() << "] "
                      << spec.resource << '\n';
        }
        patch::write_bytes(transaction.path() / "manifest.json",
                           text_bytes(patch::generate_manifest()));
        patch::write_bytes(transaction.path() / "hash_rules_data.inc",
                           text_bytes(patch::generate_hash_rules()));
        transaction.commit();
        std::cout << "All 38 payloads match 0.22.2 exactly. No game files were modified.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
