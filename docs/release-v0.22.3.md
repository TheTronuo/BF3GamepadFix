# BF3GamepadFix v0.22.3 — pre-release

This release restores native Xbox input actions and console-style campaign
prompts in Battlefield 3 on PC while retaining PC menus and graphics settings.

- Xbox 360 artwork is the default. Existing Xbox runtime behavior is preserved.
- `BF3Controller.ini` selects `PromptStyle=Xbox360` or `PromptStyle=PS3`.
  Restart the game after changing it. PS3 changes artwork; active controls remain XInput.
- Six native input profiles cover soldier, gunner, helicopter, jet, MAV and tank
  contexts, with the existing scenario event routing and QTE button dictionary.
- All 14 UI Scaling Fix v1.04 patches are integrated, with upstream MIT notices.
- Standalone C++ installer supports install, status and exact restoration of the
  previous archives and loader, including an existing UI Scaling Fix installation.
- Resource patches are distributed as reversible XOR data. Game files and
  original DLLs are not included.

The Windows ZIP contains the installer, the existing tested x86 runtime DLL,
default configuration, patch data and third-party notices. Extract the entire
ZIP beside `bf3.exe`, close the game, run `BF3GamepadFix.exe` and choose Install.
Use Remove to restore the installation's previous state. Keep the package and
its backup directory until you have removed the mod.

Supported resource build: **1147186** only. Checks reject different resource
archives and metadata before installation.

Validation: all eight offline checks passed; all 38 payloads retain their
exact identities. Installer checks cover fresh installation, repeated installation,
status, rollback, exact restore, UI Scaling Fix migration and configuration
preservation using a synthetic game fixture. The shipped
runtime DLL is byte-for-byte the existing working 0.22.3 binary. The Xbox path has
been observed working in campaign gameplay. Full campaign coverage and the new
PS3 presentation have not been verified in-game, so this is a pre-release.

This release targets single-player campaign use. Native DualShock device support
and the optional upstream `gm_fix.dll` multiplayer fixes are not included.
