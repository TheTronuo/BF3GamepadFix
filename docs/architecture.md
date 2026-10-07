# Architecture

The project separates offline resource generation from the runtime DLL. The
resource builder never loads the game DLL. The game DLL never parses release
manifests, compresses UI movies, or runs the builder.

## Runtime

`proxy.cpp` owns the exported `getBuildInfo` wrapper. It preserves incoming x86
registers and flags, initializes the mod once, and jumps to the original export.
The original DLL location is the same as in the private 0.22.2 installation.

`bootstrap.cpp` coordinates startup. `engine.cpp` owns the supported executable
identity and all runtime RVAs. The executable must have the expected PE timestamp,
image size, x86 machine type, and fixed image base. Hook targets and platform
strings also have exact byte guards.

`ui_scaling.cpp` contains the 14 retained upstream scaling patches. It checks all
expected bytes before the first write, restores page protection after writes, and
attempts rollback on an OS write failure. These addresses and replacement bytes
are identical to the previous source. See `licenses/UI-Scaling.txt`.

`hooks.cpp` installs two MinHook detours:

1. SHA1 finalization remaps only an exact digest from the 38-entry release
   whitelist to its original catalog identity. Other hashes are untouched.
2. Button-text formatting returns the native GFx frame from the active
   `PadInputActionData`. It calls the game's own String constructor and falls back
   to the original formatter when no supported pad frame exists.

`input_frames.cpp` isolates the game vector ABI, type interval, action field
offsets, and original frame names. Static assertions require the 32-bit layout.
Keyboard actions are skipped when choosing a pad frame. No label matching,
translated-text matching, or hardcoded tank/gunner keyboard aliases are used.

`hash_rules_data.inc` is a checked-in generated C++ table. Its generator lives in
the C++ patch module. Tests require regeneration to produce the exact same text.

The shared UI platform is inspected, never rewritten: it stays **win32**. Xbox
presentation is selected inside four local widgets in the offline UI patch.

### Optional PS3 presentation (0.22.3)

`config.cpp` reads `[Controller] PromptStyle` from `BF3Controller.ini`, relative
to the DLL directory. It is read once outside DllMain, after the existing runtime
hooks succeed. Xbox360 is the default, including for absent or invalid settings.
The Xbox path installs no extra hook.

For PS3, `prompt_hook.cpp` detours the guarded native GFx memory-file constructor
at RVA `0x1367f00`. Disassembly establishes the x86 `thiscall` ABI: a name pointer,
a borrowed data pointer, and a byte count, returning `this` and popping 12 bytes.
The game's derived file wrapper owns its original allocation in separate fields;
the overlay changes only the pointer passed to its base constructor.

`prompt_movie.cpp` requires the exact size and full SHA256 of either installed
Xbox library. It creates a separate immutable copy and verifies the complete PS3
output SHA256. The source buffer stays unchanged. Two cached copies remain alive
until process exit. Unrelated, corrupt or unsupported movies pass through, as do
allocation/parse/access failures. Hook installation failure retains both Xbox
hooks. No archive writes, SHA whitelist changes or global platform changes occur.

`switch_prompt_widgets_to_ps3` replaces the same four 28-byte local assignments
with `ps3`, maintaining stack balance and instruction offsets. It changes one
constant-pool operand in the PS3 QTE lookup to `m_gamepadTranslationXenon`.
Both dictionary definitions remain unchanged: the active Xbox inputs and PS3
artwork use the same physical button IDs. No native PS3 input profile is selected.
Changing the INI during a running game takes effect only on the next launch.

## Resource generation

`release_0222.cpp` is the single pinned release specification. Each resource has:

- Its game resource name, archive path, range, catalog SHA1, and before/after SHA256.
- Its expected patched SHA1 for the runtime whitelist.
- For EBX edits: byte offset, before/after signed 32-bit value, field name, and owner GUID.
- For GFx libraries: expected decoded size, with class discovery delegated to the UI module.

This is a release-specific editor. It intentionally does not claim to be a generic
EBX authoring tool or to support arbitrary game revisions.

The 36 uncompressed resource patches comprise six input maps, 14 input
configurations, and 16 scenario/tooltip resources. The input maps preserve their
Win32 primary map containers and reference the native Xbox Actions array. This is
the 0.22.1 compatibility approach carried into 0.22.2. The selected campaign
platform-event links and console input configuration references are unchanged.

`prompt_widgets.cpp` parses the original PC GFx tag stream and each class's AVM1
constant pool. It finds exactly one original platform-cache assignment in each of:

| Class | Local field |
| --- | --- |
| `Widget.Hud.QuickTimeEvent` | `m_platform` |
| `Widget.Icon3D.Interaction.InteractionTag` | `m_platform` |
| `Widget.Message.TooltipMessage` | `m_platform` |
| `Widget.Button.ConsoleButtonBar` | `m_Platform` |

Each 28-byte assignment is rebuilt with a literal `xenon` and stack-neutral AVM1
padding. Function lengths, branch offsets, constant pools, menu classes, and
all bytes outside those four spans retain their original values. Both PC UI
library variants are processed. Original Buttons and QuickTimeEvent artwork is
not part of the patch set.

`cas_blocks.cpp` owns 64 KiB CAS block framing. It tries zlib memory levels 5–9 at
compression level 9, retaining the first smallest result. Only the final zlib tail
is padded to preserve the resource's stored size. The output is decompressed again
and compared with the input movie. zlib 1.3.1 and exact output digest checks prevent
silent compressor drift.

`resource_builder.cpp` validates source identities, applies guarded edits, checks
the rebuilt payloads, and creates manifests and hash tables. The CLI publishes a
new output directory after all 38 resources succeed. It refuses an existing output
directory and an output inside the source tree.

## Future changes

For a new release, update the pinned specification, regenerate the hash table with
the C++ builder, and validate against new local reference fixtures. Runtime
addresses stay centralized in `engine.hpp`; controller frame selection stays in
`input_frames.cpp`; UI presentation selectors stay in `prompt_widgets.cpp`.

Native DualShock input and alternate controller layouts are not implemented.
The configuration changes prompt artwork while preserving Xbox/XInput actions.
