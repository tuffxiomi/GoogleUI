# GoogleUI source analysis

## Target inputs inspected

The supplied Minecraft library is AArch64 ELF and its SHA-256 is:

`444e77434bdd3789a0d90978d06336a99831e78e52955e528258cc375dfa0557`

The supplied target is Minecraft `26.45.1` according to the provided source compatibility data.

## BedrockTools architecture observed

The BedrockTools tree contains a signature registry, grouped offset headers, SDK wrappers, hooks, and 38 feature modules. Its feature modules are grouped as HUD, misc, player, and visual. The GoogleUI package intentionally does not copy those directories or headers.

### Signature registry

`include/bedrocktools/memory/Signatures.hpp` defines 121 usable signature IDs plus the terminal `Count` entry. The registry covers version/name-tag lookup, world time and weather, actor/game-mode actions, rendering and tessellation, UI text and screens, inventory/item functions, packet/network handling, skin data, block queries, and other game internals.

### Offset groups

The supplied source has six major offset groups:

- `sdk/offsets/Core.hpp` — 16 constants for client/region, block source, render material, hover text, UI render context, client instance, item helpers, and level renderer.
- `sdk/offsets/Network.hpp` — 19 constants for packet/network metadata and payload fields.
- `sdk/offsets/Render.hpp` — 54 constants for fog, camera/shader state, tessellation, render materials, matrices, blocks, and render buffers.
- `sdk/offsets/Skin.hpp` — 11 constants for skin implementation/image/persona/animation fields.
- `sdk/offsets/UI.hpp` — 23 constants for reserved UI regions, item/compound-tag fields, cursor data, and render contexts.
- `sdk/offsets/World.hpp` — 37 constants for actor/world state, names, skin/entity data, motion/rotation, level/dimension, hurt/fuse timing, name-tag flags, and typed values.

These offsets are source-level BedrockTools SDK knowledge. They are not needed by GoogleUI because GoogleUI does not directly manipulate Minecraft actor/world/render structures.

## ChunkBaseUI implementation reused conceptually

The supplied ChunkBaseUI source is a self-contained external Levi mod. Its important path is:

1. Register the mod through `PLGetModRegistration()`.
2. Resolve LeviLauncher symbols from `libpreloader.so`.
3. Hook the external-mod JNI bridge using byte signatures with wildcarded ARM64 immediate/relocation bytes.
4. Receive draw commands and append a custom floating button.
5. Open an Android WebView through JNI/activity helpers.
6. Inject the WebView client and optional filtering data.

The bridge signatures found in the supplied source are:

- `nativeGetExternalModsInfo` — 32-byte wildcard signature; expected hook size 464.
- `nativeSetExternalModConfig` — 36-byte wildcard signature; expected hook size 316.
- `nativeGetDrawCommands` — 24-byte wildcard signature; expected hook size 2416.
- `nativeGetDrawCommandsRevision` — 16-byte target signature with wildcarded branch/relocation bytes; the supplied target RVA is `0x6e5e0` and the small stub RVA is `0x621b4`.
- `nativeToggleExternalMod` — optional 32-byte wildcard signature; expected hook size 128.
- `nativeGetExternalButtonCount` — 24-byte wildcard signature for the newer button bridge.
- `nativeGetExternalButtonInfo` — 24-byte signature; expected function size 460.
- `nativeGetExternalButtonIconBytes` — 24-byte signature.
- `nativeDispatchExternalButtonEvent` — 24-byte wildcard signature.

GoogleUI keeps only this Levi external-bridge strategy. It has no BedrockTools signature registry, no BedrockTools offsets, and no BedrockTools feature modules.

## GoogleUI changes

- Module identity: `GoogleUI` / `google_ui`.
- Main URL: `https://www.google.com`.
- WebView client/data namespaces are `google_ui` / `org.levimc.googleui`.
- Button icon is `resources/google_button.png`.
- Package entry is `libgoogle_ui.so`.
- Target selector is `Minecraft 26.45.*`.
- The package contains only GoogleUI runtime/config/resource files; the BedrockTools source tree is not embedded.

## Verification performed

- 12/12 static QA checks pass.
- All 21 C++ translation units pass host `clang++ -std=c++20 -fsyntax-only` parsing with the project's local headers.
- Embedded DEX and ad-block data generation completes successfully.
- The exact supplied `libpreloader.so` binary is not present in the uploads, so a real AArch64 link/package build cannot be honestly claimed from this environment. The build script therefore verifies the expected SHA-256 and refuses to link against an unknown preloader.
