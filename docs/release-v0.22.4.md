# BF3GamepadFix v0.22.4 — pre-release

Installation now uses one downloadable **BF3GamepadFix.exe** with a compact
English window. The installer contains the runtime and patch data and finds
Battlefield 3 through its own location, EA installation records or Steam libraries.

1. Close Battlefield 3.
2. Download and open **BF3GamepadFix.exe** from Assets below.
3. Select **Xbox 360** or **PlayStation 3**, then click **Install**.
4. Once the button reads **Installed**, launch the game normally.

If the game is not found, click **Select game...**, select `bf3.exe`, then click
**Install**. Keep the installer wherever you downloaded it; no ZIP extraction or
manual file copying is needed.

Reopen the installer to switch prompt style. Selecting Xbox 360 or PlayStation 3
updates the configuration immediately; restart the game to apply it. Both modes
use XInput controls. PlayStation controllers must be presented to the game as XInput.

The Install button stays disabled after installation. Controller icons retain
their appearance when clicked. Command mode remains available for install,
read-only status and restoring the previous installation:

```powershell
.\BF3GamepadFix.exe remove "C:\Games\Battlefield 3"
```

Replace the path with your game directory and close the game first. Keep the game's
`ControllerMod/BF3GamepadFix` backup folder until uninstalling.

Requires **64-bit Windows** and resource build **1147186**. The tested 0.22.3 runtime
DLL and all 38 resource patches are retained exactly; this update changes the
installer and documentation. Camera sensitivity and response settings are not
modified. UI Scaling Fix v1.04 remains included.

Validation covers embedded payload checksums, prompt selection and INI preservation,
transaction rollback, exact restore and the existing install/status/restore fixture
suite. The GUI has also been used for a local installation. Full campaign
verification is ongoing, so this remains a pre-release.

`SHA256SUMS.txt` contains the downloadable EXE checksum. The automatically generated
source archives are for developers and do not contain the compiled installer.
