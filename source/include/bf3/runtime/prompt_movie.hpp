#pragma once

#include "bf3/patch/prompt_widgets.hpp"

namespace bf3::runtime {
// Empty bytes means this is not one of the two exact installed Xbox libraries.
// Throws on malformed supported data; callers retain the original movie.
patch::PromptLibrary make_ps3_prompt_movie(const std::uint8_t* bytes, std::size_t size);
} // namespace bf3::runtime
