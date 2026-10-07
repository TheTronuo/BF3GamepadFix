#include "bf3/runtime/prompt_movie.hpp"
#include "bf3/hash.hpp"

namespace bf3::runtime {
patch::PromptLibrary make_ps3_prompt_movie(const std::uint8_t* bytes, std::size_t size) {
    if (!bytes || (size != 726805 && size != 662290))
        return {};
    const auto* expected = size == 726805
                               ? "4863d5f29a27410842ce649f7ab9babb20295c176d2d5411cd8a1a1537a3ef98"
                               : "dff5d2d030e299a10d9a3295ed2bf9694d7dbe0394fb1efd553d28f639412946";
    if (sha256(bytes, size) != expected)
        return {};
    auto result = patch::switch_prompt_widgets_to_ps3(Bytes(bytes, bytes + size));
    const auto* golden = size == 726805
                             ? "7f2026fa5fefe6e2734bfc27128a13a25f80b2bd7f050ffc105e2b9bbc9d7691"
                             : "d4c974ebb00532e266fd7673b49db9fecc7002bcf582b8994709a8712fa1ae23";
    if (sha256(result.bytes) != golden)
        throw std::runtime_error("PS3 presentation output identity mismatch");
    return result;
}
} // namespace bf3::runtime
