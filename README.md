# BF3GamepadFix

**0.22.3 pre-release**: native campaign gamepad controls and configurable
Xbox 360 / PS3 prompt artwork for Battlefield 3 on PC, based on the
existing **0.22.2-PC-menu-Xbox-prompts** mod for
Battlefield 3 on Windows. The mod routes the campaign's native Xbox input actions
and prompt widgets while retaining the shared Win32 menu platform and UI scaling.

The runtime DLL and resource generator are both written in **C++17**. Building
or regenerating the mod requires no Python. MinHook and zlib are external C
libraries; their source is not vendored in this repository.

**Status:** the generator still reproduces all 38 existing Xbox payloads exactly.
Configuration, isolated runtime hook tests and independent AVM1 checks pass.
The new PS3 presentation still requires an in-game visual test.

## Install the pre-release

Download the Windows ZIP from [Releases](https://github.com/tronuo0/BF3GamepadFix/releases).
Extract **all contents** into the Battlefield 3 folder containing `bf3.exe`.
Close BF3, run `BF3GamepadFix.exe`, then choose **1 Install**. The default prompts
are Xbox 360. The installer is a 64-bit Windows application; the game's DLL is
32-bit. No Python or Visual Studio installation is needed to use the release.

For a different game directory or a terminal:

```powershell
.\BF3GamepadFix.exe install "F:\Games\Battlefield 3"
.\BF3GamepadFix.exe status "F:\Games\Battlefield 3"
.\BF3GamepadFix.exe remove "F:\Games\Battlefield 3"
```

**Remove** restores the resource archives and loader to their state before this
installation. Existing `BF3Controller.ini` settings are preserved. Keep the
extracted package and `ControllerMod/BF3GamepadFix` backup directory for restore.
Older controller mods that change different resource bytes must be removed first.

Only build **1147186** with the pinned resource identities is supported. The
installer checks the complete three archives, signed metadata, all 38 resource
ranges and the DLL before it writes. Unsupported installations are rejected.
The package contains XOR patch data; original game assets and original game DLLs
are read from the user's own installation and are not distributed.

## Included UI fix

All 14 UI patches from
[BF3-UI-Scaling-Fix v1.04](https://github.com/IlIHydraIlI/BF3-UI-Scaling-Fix)
are already integrated: scaling, minimap, nametags, 3D icons, Commo Rose, text
flickering, killfeed and capture progress. A separate UI Scaling Fix DLL is not
needed. The installer also accepts that mod's original-DLL backup named
`ori_Engine.BuildInfo_Win32_Retail_dll.dll` and preserves the previous loader for
restore. The optional `gm_fix.dll` gameplay fixes are not included or loaded.
Credits and MIT notices are retained in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

## Prompt configuration

Place `BF3Controller.ini` beside the game's mod DLL and `bf3.exe`:

```ini
[Controller]
PromptStyle=Xbox360
```

Set `PromptStyle=PS3` for native PlayStation artwork. Restart BF3 after changing
the file. Missing files/keys and invalid values select Xbox360; invalid values
are recorded in `ControllerMod/loader-diagnostic.log`.

Xbox360 uses the existing two runtime hooks and unchanged Xbox resources. The
optional presentation hook is installed only for PS3. It supplies private copies
of the two exact supported UI libraries while the game loads them. CAS archives,
input maps, scenario events, the shared Win32 menu platform and UI scaling stay
unchanged. PS3 QTE artwork uses the Xbox physical-button dictionary so the required
press matches the active Xbox/XInput controls, including shoulders and triggers.

This setting selects artwork, not native DualShock device support or a different
control layout. The controller must already be available to BF3 through XInput.

## Requirements

- Windows, Visual Studio 2022 with the C++ workload and Windows SDK.
- CMake 3.24 or newer.
- An external MinHook source directory for the runtime.
- zlib **1.3.1**, with headers and a library matching the generator's architecture.
- Original files from the supported BF3 build **1147186** when generating resources.

Game files, original DLLs, crash dumps, generated payloads, and private installation
tools are kept outside this source project.

## Build

From the project directory, in PowerShell:

```powershell
# The game DLL is always 32-bit.
cmake -S . -B build-runtime -A Win32 `
  -DBF3_BUILD_TOOLS=OFF `
  -DBF3_MINHOOK_SOURCE_DIR=C:/deps/minhook
cmake --build build-runtime --config Release
ctest --test-dir build-runtime -C Release --output-on-failure

# The standalone generator can be 64-bit, with a matching zlib library.
cmake -S . -B build-tools -A x64 `
  -DBF3_BUILD_RUNTIME=OFF `
  -DZLIB_INCLUDE_DIR=C:/deps/zlib-1.3.1/include `
  -DZLIB_LIBRARY=C:/deps/zlib-1.3.1/lib/zlib.lib
cmake --build build-tools --config Release
ctest --test-dir build-tools -C Release --output-on-failure
```

The dependency paths are examples: replace them with your own. If zlib is shared,
its DLL must be on the executable's search path. The runtime output is
`build-runtime/Release/Engine.BuildInfo_Win32_Retail_dll.dll`; the generator is
`build-tools/Release/bf3-resource-builder.exe`.

A single Win32 build of both targets is also supported when a 32-bit zlib is
available. See [build and verification details](docs/build.md).

## Generate resources

```powershell
build-tools/Release/bf3-resource-builder.exe `
  --game C:/Games/Battlefield3 `
  --output C:/BuildArtifacts/BF3-0.22.2
```

`--game` expects unmodified supported game files. It verifies three full archives
and 15 metadata files, reads the 38 original resources, and builds new payloads
in a new output directory. It does not install the mod or change the game.

For a saved local release fixture containing the original resources:

```powershell
build-tools/Release/bf3-resource-builder.exe `
  --originals C:/PrivateFixtures/pc-menu-xbox-prompts-stage `
  --output C:/BuildArtifacts/BF3-0.22.2-from-fixture
```

Every output must match the pinned size, SHA256, and SHA1. The generator publishes
the output directory only after all resources pass. Output includes originals,
patched payloads, an installation manifest, and the generated runtime hash table.

## Source layout

| Location | Responsibility |
| --- | --- |
| `include/bf3` | Public module interfaces and bounded binary helpers |
| `src/runtime` | DLL proxy, executable guards, hooks, native prompt frames, UI scaling |
| `src/patch` | Release specification, CAS blocks, local GFx assignments, manifest generation |
| `src/crypto` | SHA1/SHA256 through Windows CNG |
| `tools` | Standalone C++ resource-builder CLI |
| `src/install` | Guarded archive and DLL transaction with rollback |
| `tests` | Unit checks, release parity, isolated DLL proxy checks |
| `docs` | Architecture, build instructions, verification scope |
| `licenses` | MinHook and upstream UI scaling notices |

The C++ installation/restore utility is included in this source project.
Private research and packaging scripts are not published. The DLL retains
its original installation contract: it loads
`ControllerMod/loader/original-buildinfo.dll` relative to itself and writes its
log under `ControllerMod`.

See [architecture](docs/architecture.md) and [verified results](docs/verification.md).
