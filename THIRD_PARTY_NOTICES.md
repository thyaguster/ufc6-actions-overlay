# Credits and third-party notices

UFC 6 Actions Overlay is released under **GPL-2.0-only**; see [LICENSE](LICENSE). The installer, overlay, controller hooks, and verification fixtures are supplied as source in this repository and in the full release ZIP.

## Vulkan headers

The bundled headers under `vendor/vulkan-headers/include` come from [Khronos Vulkan-Headers](https://github.com/KhronosGroup/Vulkan-Headers), header revision 368. Their original copyright/SPDX notices are retained. See `vendor/vulkan-headers/LICENSE.md` and the full license texts in `vendor/vulkan-headers/LICENSES` for the applicable Apache-2.0 and MIT terms. These terms apply to the third-party headers, independently of the overlay's GPL license.

## Compatibility research

- [KytyPS5](https://github.com/KytyPS5/KytyPS5) provided public upstream reference material. This mod does not distribute the emulator, launcher, or the user's game executable.
- The public [KytyPS5 two-player patch](https://github.com/Danteh3-lab/kytyps5-2player-patch) independently documents the supported `3309ab23f` engine and its controller function locations. That project is a research reference; its binary is not bundled in this release.
- The Windows setup uses the system .NET Framework. The Vulkan layer uses the game's existing Vulkan loader. The release does not redistribute either runtime.

This is an unofficial community tool, not affiliated with EA, Sony, Microsoft, PORTHEON, Khronos, or the KytyPS5 project. The license covers the tool, not UFC 6 or other third-party content.
