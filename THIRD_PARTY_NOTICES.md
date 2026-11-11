# Credits and third-party notices

UFC 6 Actions Overlay is released under **GPL-2.0-only**; see [LICENSE](LICENSE). The installer, overlay, controller hooks, and verification fixtures are supplied as source in this repository and in the full release ZIP.

## Vulkan headers

The bundled headers under `vendor/vulkan-headers/include` come from [Khronos Vulkan-Headers](https://github.com/KhronosGroup/Vulkan-Headers), header revision 368. Their original copyright/SPDX notices are retained. See `vendor/vulkan-headers/LICENSE.md` and the full license texts in `vendor/vulkan-headers/LICENSES` for the applicable Apache-2.0 and MIT terms. These terms apply to the third-party headers, independently of the overlay's GPL license.

## Compatibility research

- [KytyPS5](https://github.com/KytyPS5/KytyPS5) provided public upstream reference material. This mod does not distribute the emulator, launcher, or the user's game executable.
- The public [KytyPS5 two-player patch](https://github.com/Danteh3-lab/kytyps5-2player-patch) independently documents the supported `3309ab23f` engine and its controller function locations. That project is a research reference; its binary is not bundled in this release.
- The Windows setup uses the system .NET Framework. The Vulkan layer uses the game's existing Vulkan loader. The release does not redistribute either runtime.

This is an unofficial community tool, not affiliated with EA, Sony, Microsoft, PORTHEON, Khronos, or the KytyPS5 project. The license covers the tool, not UFC 6 or other third-party content.

## User-provided threading runtime

The startup bridge declares API forwarders for the engine's `libwinpthread-1.dll`. The original winpthreads DLL is not distributed. Setup verifies the user's installed original, preserves its bytes under `ufc6_pthread_original.dll`, and restores it on uninstall. Forwarder tests use a tiny source-built stand-in; it contains no original runtime or game bytes.

## UFC 6 Xbox Controller Icons

The asset-free `patcher_core.py` and version profile under `vendor/xbox-icons` come from [thyaguster/ufc6-xbox-icons](https://github.com/thyaguster/ufc6-xbox-icons), v1.0.1. They retain their MIT license and notices. Replacement artwork is generated exclusively from the user's own game files. No game assets or restore backups are distributed. The helper bundles CPython and PyInstaller; see `vendor/xbox-icons/THIRD-PARTY-NOTICES.md` and `runtime-licenses/` for the complete runtime terms.

## Controller prompt artwork

The SVG artwork under `src/assets/controller-prompts` is a selected subset of [Kenney Input Prompts 1.5A](https://kenney.nl/assets/input-prompts), distributed under Creative Commons Zero (CC0). The original license is included in that directory. The exact authored paths are compiled into `src/controller_artwork_data.h` by `src/generate_controller_artwork.py` and drawn as vectors. White face-button artwork is tinted to the overlay's Xbox/PlayStation prompt colors; authored direction highlights are preserved. No game artwork is bundled for these overlay prompts.
