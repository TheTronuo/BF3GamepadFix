# BF3GamepadFix

| Feature | Status |
| :--- | :---: |
| **PlayStation Prompts** | ✅ Supported |
| **Xbox 360 Prompts** | ✅ Supported |
| **Game Version** | Build **1147186** |

Gamepad controls and Xbox 360 / PlayStation button prompts for the **Battlefield 3 single-player campaign on PC**. Keeps the PC menus and graphics settings, with UI scaling included.

**Version 0.22.3 is a pre-release.** Both prompt styles have been confirmed in gameplay; full campaign verification is ongoing.

<details>
<summary><strong>In-game preview — click to expand</strong><br><br></summary>

<p align="center">
  <img src="assets/screenshots/playstation-prompts.png" alt="Battlefield 3 campaign with PlayStation R2, L2 and Square button prompts" width="960">
  <br><em>PlayStation prompts</em>
</p>

<p align="center">
  <img src="assets/screenshots/xbox360-prompts.png" alt="Battlefield 3 campaign with Xbox 360 RT, LT and X button prompts" width="960">
  <br><em>Xbox 360 prompts</em>
</p>

</details>

## Install

1. Close the game.
2. Download the ready-to-use Windows ZIP from [Releases](https://github.com/tronuo0/BF3GamepadFix/releases). The automatically generated source archives do not contain the compiled mod.
3. Extract **all contents** into the game folder, next to `bf3.exe`.
4. Run `BF3GamepadFix.exe`, choose **1 Install**, then launch the game normally.

To uninstall, close the game and choose **2 Restore previous installation** in the same installer. Keep the extracted package and the `ControllerMod/BF3GamepadFix` backup folder for restore.

## Settings

Edit `BF3Controller.ini` next to `bf3.exe`, then restart the game:

```ini
[Controller]
PromptStyle=Xbox360
```

| Value | Button prompts |
| :--- | :--- |
| `Xbox360` | Xbox 360 (default) |
| `PS3` | PlayStation |

This changes the button icons; controls use XInput in both modes. PlayStation controllers must be available to the game through XInput.

## Credits

Includes [BF3-UI-Scaling-Fix v1.04](https://github.com/IlIHydraIlI/BF3-UI-Scaling-Fix). A separate UI Scaling Fix installation is not required. See [third-party notices](THIRD_PARTY_NOTICES.md) for credits and licenses.

[Build instructions](docs/build.md) · [Verification details](docs/verification.md)
