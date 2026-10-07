# Single-EXE installer

The Windows GUI embeds the verified runtime, 38 reversible XOR patches, default
configuration and third-party notices. The player downloads only
`BF3GamepadFix.exe`; the installer needs no adjacent package folder.

## Build

Configure from the repository root:

```powershell
cmake -S . -B build-gui -A x64 `
  -DBF3_BUILD_RUNTIME=OFF -DBF3_BUILD_TOOLS=OFF `
  -DBF3_PACKAGE_DIR=C:/path/to/verified/BF3GamepadFix
cmake --build build-gui --config Release
ctest --test-dir build-gui -C Release --output-on-failure
```

`BF3_PACKAGE_DIR` contains `runtime/`, `patches/`, `BF3Controller.ini`,
`THIRD_PARTY_NOTICES.md` and `licenses/`. The runtime checksum must match
`BF3_RELEASE_DLL_SHA256`. CMake embeds a checksum for every payload file.

The installer extracts its payload into a unique temporary directory, checks all
payload checksums, uses the existing transaction engine and cleans up the
temporary payload. Restore uses the game's `ControllerMod/BF3GamepadFix` record
and previous loader together with the embedded patches.

## Interface and game discovery

The client area is 368 x 52 logical pixels, with Windows DPI scaling. Install is
on the left, followed by native radio controls, 24 x 16 controller icons and
Xbox 360 / PlayStation 3 labels. All UI text is English. Separate icon and radio
rectangles prevent selection repaint from covering the icons; icon and label
clicks also select the corresponding option.

Discovery checks the installer's folder, working directory, EA registry entries
and Steam library folders, including the BF3 app manifest. The first discovered
installation is selected. When discovery fails, the main button opens a native
`bf3.exe` picker. To target another installation explicitly, put the EXE beside
that installation's `bf3.exe` or use a command below with its game directory.

After installation, the button reads **Installed** and is disabled, including on
later launches when an active installation record exists. Switching prompt style
then writes and verifies `PromptStyle` immediately while preserving other INI
settings. Failed writes restore the previous selection and show an error. The
running game reads the new style on its next start.

The recorded state controls the button; full archive and metadata checks happen
when installing, restoring or running `status`. Errors show the backend's English
diagnostic text. The runtime and resource patches are the existing 0.22.3 payload;
this installer update does not alter camera settings.

## Command mode

The compact GUI has one primary action. The same EXE also supports:

```powershell
.\BF3GamepadFix.exe install "C:\Games\Battlefield 3"
.\BF3GamepadFix.exe status "C:\Games\Battlefield 3"
.\BF3GamepadFix.exe remove "C:\Games\Battlefield 3"
```

Without an explicit game directory, command mode uses the EXE's directory. Output
goes to the terminal or redirected handles when available, otherwise to a Windows
message box. The process returns the backend's exit code. `status` is read-only.

## Preview and checks

For a guaranteed read-only design review, configure a separate build with
`-DBF3_PREVIEW_ONLY=ON`. The regular build also supports `--preview`. Preview
builds simulate the Installed button state and never write game configuration;
their command mode rejects game operations.

`--self-test` validates every embedded file and tests prompt switching,
preservation of other INI sections/keys and failed configuration writes in an
isolated temporary directory. Transaction and CLI fixture checks cover install,
rollback and exact restore. The approved GUI was also used for a local installation
and checked for icon repaint and Installed state behavior.

The installer has no automatic updates, automatic elevation or persistent
recovery journal for process termination or power loss.
