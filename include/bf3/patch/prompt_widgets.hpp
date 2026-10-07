#pragma once

#include "bf3/bytes.hpp"
#include <string>
#include <vector>

namespace bf3::patch {
struct PromptEdit {
    std::string class_name;
    std::string field;
    std::size_t offset;
    Bytes before;
    Bytes after;
};

struct PromptLibrary {
    Bytes bytes;
    std::vector<PromptEdit> edits;
};

// Rebuilds the four local assignments from each class's own constant pool.
PromptLibrary patch_prompt_widgets(const Bytes& original);

// Presentation-only overlay of an existing Xbox movie. QTEs keep the Xbox
// physical-button dictionary while using the original PS3 artwork.
PromptLibrary switch_prompt_widgets_to_ps3(const Bytes& xbox);
} // namespace bf3::patch
