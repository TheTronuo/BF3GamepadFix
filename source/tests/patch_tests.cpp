#include "bf3/hash.hpp"
#include "bf3/patch/cas_blocks.hpp"
#include "bf3/patch/resource_builder.hpp"
#include "bf3/runtime/prompt_movie.hpp"
#include <fstream>
#include <iostream>
#include <iterator>

namespace {
void expect(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}

template <class Fn> void rejects(Fn function, const char* message) {
    try {
        function();
    } catch (const std::exception&) {
        return;
    }
    throw std::runtime_error(message);
}

std::string read_text(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file)
        throw std::runtime_error("Cannot read generated header");
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}
} // namespace

int wmain(int argc, wchar_t** argv) {
    try {
        namespace patch = bf3::patch;
        const bf3::Bytes abc{'a', 'b', 'c'};
        expect(bf3::sha1(abc) == "a9993e364706816aba3e25717850c26c9cd0d89d", "SHA1 known vector");
        expect(bf3::sha256(abc) ==
                   "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
               "SHA256 known vector");
        expect(patch::release_resource_count() == 38, "Expected all 38 release resources");
        expect(argc >= 2 && patch::generate_hash_rules() == read_text(argv[1]),
               "Generated runtime whitelist drift");
        bf3::Bytes raw(70001);
        for (std::size_t i = 0; i < raw.size(); ++i)
            raw[i] = static_cast<std::uint8_t>((i * 17) % 251);
        const auto packed = patch::pack_cas_fixed(raw, 3000);
        expect(patch::unpack_cas(packed, raw.size()) == raw, "Multi-block CAS roundtrip");
        rejects([&] { patch::pack_cas_fixed(raw, 1); }, "CAS size-budget guard missing");
        auto truncated = packed;
        truncated.pop_back();
        rejects([&] { patch::unpack_cas(truncated, raw.size()); }, "Truncated CAS block accepted");
        rejects([&] { patch::unpack_cas(packed, 1); }, "CAS decoded-size guard missing");
        rejects([&] { patch::patch_prompt_widgets(abc); }, "Malformed GFx accepted");
        rejects([&] { patch::rebuild_resource(patch::release_resources()[0], abc); },
                "Wrong original resource accepted");
        if (argc == 3) {
            const std::filesystem::path stage(argv[2]);
            std::size_t assignments = 0;
            for (std::size_t i = 0; i < patch::release_resource_count(); ++i) {
                const auto& spec = patch::release_resources()[i];
                const auto original =
                    patch::read_resource(stage / spec.original_file, 0, spec.stored_size);
                const auto expected =
                    patch::read_resource(stage / spec.patched_file, 0, spec.stored_size);
                const auto rebuilt = patch::rebuild_resource(spec, original);
                expect(rebuilt.bytes == expected,
                       "Rebuilt payload is not byte-identical to 0.22.2");
                auto wrong = original;
                wrong[0] ^= 1;
                rejects([&] { patch::rebuild_resource(spec, wrong); },
                        "Corrupted original accepted");
                if (spec.decoded_size) {
                    const auto movie = patch::unpack_cas(original, spec.decoded_size);
                    const auto result = patch::patch_prompt_widgets(movie);
                    expect(result.edits.size() == 4, "Four local prompt assignments required");
                    assignments += result.edits.size();
                    auto missing_assignment = movie;
                    missing_assignment[result.edits[0].offset] ^= 1;
                    rejects([&] { patch::patch_prompt_widgets(missing_assignment); },
                            "Missing original assignment accepted");
                    for (std::size_t offset = 0; offset < movie.size(); ++offset) {
                        bool allowed = false;
                        for (const auto& edit : result.edits) {
                            expect(edit.before.size() == 28 && edit.after.size() == 28,
                                   "GFx branch offsets changed");
                            allowed |= offset >= edit.offset && offset < edit.offset + 28;
                        }
                        expect(allowed || movie[offset] == result.bytes[offset],
                               "Unrelated UI byte changed");
                    }
                    const auto xbox = patch::unpack_cas(expected, spec.decoded_size);
                    const auto ps3 = bf3::runtime::make_ps3_prompt_movie(xbox.data(), xbox.size());
                    expect(ps3.bytes.size() == xbox.size() && ps3.edits.size() == 5,
                           "PS3 must change only four widgets and one QTE lookup");
                    for (std::size_t offset = 0; offset < xbox.size(); ++offset) {
                        bool allowed = false;
                        for (const auto& edit : ps3.edits) {
                            expect(edit.before.size() == edit.after.size(), "PS3 GFx size drift");
                            allowed |=
                                offset >= edit.offset && offset < edit.offset + edit.before.size();
                        }
                        expect(allowed || xbox[offset] == ps3.bytes[offset],
                               "PS3 modified unrelated UI code");
                    }
                    expect(bf3::runtime::make_ps3_prompt_movie(ps3.bytes.data(), ps3.bytes.size())
                               .bytes.empty(),
                           "Overlay accepted something other than exact Xbox library");
                    auto corrupt_xbox = xbox;
                    corrupt_xbox.back() ^= 1;
                    expect(bf3::runtime::make_ps3_prompt_movie(corrupt_xbox.data(),
                                                               corrupt_xbox.size())
                               .bytes.empty(),
                           "PS3 overlay accepted corrupt Xbox source");
                    std::cout << "PS3 movie " << spec.decoded_size
                              << " SHA256=" << bf3::sha256(ps3.bytes) << '\n';
                }
            }
            expect(assignments == 8, "Both PC UI library variants must be covered");
            std::cout << "38/38 resources byte-identical; eight local UI assignments; all "
                         "unrelated UI bytes preserved.\n";
        }
        std::cout << "Patch tests passed: hashes, whitelist generation, CAS bounds, GFx guards, "
                     "source identity.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
