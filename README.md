# GoogleUI v1.0.0

GoogleUI is an Android ARM64 LeviLauncher native mod that opens `https://www.google.com` in an in-game WebView from a floating button. It keeps the responsive layout, JNI lifecycle handling, WebView cleanup, and external Mod Menu bridge used by the supplied reference project, and ships without any third-party framework source tree.

## Target

The supplied Minecraft library matches the 26.45.1 target fingerprint in `compatibility/target.json`. The supplied reference project also pins LeviLauncher 1.5.18's `libpreloader.so` fingerprint there.

GoogleUI itself does not patch `libminecraftpe.so` and does not resolve game-side signatures or offsets. Its native bridge hooks are against the LeviLauncher external-mod bridge in `libpreloader.so`.

## Runtime behavior

The main floating button opens Google at `https://www.google.com`.

The WebView uses responsive sizing and a lightweight ad/tracker filter. It also has a separate configurable website action exposed through the external Mod Menu bridge; the default for that action is Google.

Turning GoogleUI off destroys both WebViews and releases their JNI references on the main thread.

## Build

Set `PRELOADER_SO` to the exact ARM64 `libpreloader.so` used by the target LeviLauncher installation, then run:

```bash
PRELOADER_SO=/absolute/path/to/libpreloader.so ./scripts/build-arm64.sh
```

The output is `build-arm64/libGoogleUI_V1.0.0.levipack`.

The repository does not bundle `libpreloader.so` or `libminecraftpe.so`.


## GitHub Actions

The included workflow builds on `ubuntu-24.04` with Android NDK `27.2.12479018`. Run **Build GoogleUI** manually and provide a direct HTTPS URL for the exact ARM64 `libpreloader.so` whose SHA-256 is `8d628e4251498a867f9058c045068207616af170dde7b0962528fd4d54e47fed`. The workflow rejects any other preloader binary.

The source upload did not include that preloader binary, so this project does not bundle it. This avoids silently linking against an incompatible LeviLauncher build.
