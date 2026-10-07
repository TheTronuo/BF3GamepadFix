#include "bf3/runtime/input_frames.hpp"
#include <iterator>

namespace bf3::runtime {
namespace {
constexpr const char* button_frames[] = {
    "IDB_Lup",   "IDB_Ldown",  "IDB_Lleft",    "IDB_Lright",   "IDB_Rup",       "IDB_Rdown",
    "IDB_Rleft", "IDB_Rright", nullptr,        nullptr,        "IDB_Lthumb",    "IDB_Rthumb",
    "IDB_start", "IDB_alt",    "IDB_Ltrigger", "IDB_Rtrigger", "IDB_Ltrigger2", "IDB_Rtrigger2"};
constexpr const char* axis_frames[] = {
    "IDA_Axis0X", "IDA_Axis0Y", "IDA_Axis0XPos", "IDA_Axis0YPos", "IDA_Axis0XNeg", "IDA_Axis0YNeg",
    "IDA_Axis1X", "IDA_Axis1Y", nullptr,         nullptr,         nullptr,         nullptr};
using GetType = BYTE*(__thiscall*)(BYTE*);
} // namespace

const char* pad_frame(unsigned button, unsigned axis, unsigned pov) noexcept {
    if (button < std::size(button_frames) && button_frames[button])
        return button_frames[button];
    if (button != 60)
        return nullptr;
    if (axis < std::size(axis_frames) && axis_frames[axis])
        return axis_frames[axis];
    (void)pov;
    return nullptr;
}

const char* choose_native_pad_frame(const InputBindingVector* bindings, WORD first_type,
                                    WORD last_type) {
    if (last_type < first_type || (!first_type && !last_type) || !bindings || !bindings->first ||
        !bindings->last)
        return nullptr;
    const auto first = reinterpret_cast<DWORD>(bindings->first);
    const auto last = reinterpret_cast<DWORD>(bindings->last);
    if (last < first || (last - first) % sizeof(InputBindingPair) ||
        last - first > 256 * sizeof(InputBindingPair))
        return nullptr;
    for (auto* entry = bindings->first; entry != bindings->last; ++entry) {
        auto* action = entry->action;
        if (!action)
            continue;
        const auto get_type = *reinterpret_cast<GetType*>(*reinterpret_cast<void**>(action));
        auto* type = get_type(action);
        if (!type)
            continue;
        const auto id = *reinterpret_cast<WORD*>(type + 0x14);
        if (id < first_type || id > last_type)
            continue;
        const auto frame = pad_frame(*reinterpret_cast<DWORD*>(action + 0x14),
                                     *reinterpret_cast<DWORD*>(action + 0x0c),
                                     *reinterpret_cast<DWORD*>(action + 0x18));
        if (frame)
            return frame;
    }
    return nullptr;
}
} // namespace bf3::runtime
