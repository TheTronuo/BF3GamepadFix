#pragma once

#include <windows.h>

namespace bf3::runtime {
// This is the game's 32-bit ABI, not a STL container.
struct InputBindingPair {
    BYTE* action;
    DWORD modifier;
};
struct InputBindingVector {
    InputBindingPair* first;
    InputBindingPair* last;
    DWORD capacity;
    DWORD allocator;
};
static_assert(sizeof(InputBindingPair) == 8, "Runtime must be built for x86");
static_assert(sizeof(InputBindingVector) == 16, "Unexpected game vector ABI");

const char* pad_frame(unsigned button, unsigned axis, unsigned pov) noexcept;
const char* choose_native_pad_frame(const InputBindingVector* bindings, WORD first_type,
                                    WORD last_type);
} // namespace bf3::runtime
