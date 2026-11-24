# ImagoEngine — Library Versions

Triplet: `x64-mingw-dynamic`
Source: vcpkg manifest install, captured from latest `CMake: Configure` output

## Direct Dependencies

These are the libraries explicitly listed in `vcpkg.json`.

| Library | Version |
|---|---|
| SFML (core, window, network, graphics, audio) | 3.0.2 |
| SDL2 | 2.32.10 |
| imgui | 1.91.9 |
| imgui-sfml | 3.0#2 |
| nlohmann-json | 3.12.0#2 |
| yaml-cpp | 0.9.0 |
| spdlog (core, tz-offset, fmt) | 1.17.0 |
| glm | 1.0.3 |
| Catch2 | 3.13.0#1 |
| stb | 2024-07-29#1 |
| fmt | 12.1.0 |

## Transitive Dependencies

These were pulled in automatically by the libraries above and are not listed directly in `vcpkg.json`.

| Library | Version | Pulled In By |
|---|---|---|
| freetype (core, zlib, png, bzip2, brotli) | 2.13.3 | SFML |
| libpng | 1.6.55 | freetype / SFML |
| libflac (core, stack-protector) | 1.5.0 | SFML (audio) |
| libogg | 1.3.6#1 | SFML (audio) |
| libvorbis | 1.3.7#4 | SFML (audio) |
| miniaudio | 0.11.25 | SFML (audio) |
| opengl | 2022-12-04#3 | SFML (graphics) |
| opengl-registry | 2026-01-26 | SFML (graphics) |
| egl-registry | 2025-05-27 | SFML (graphics) |
| brotli | 1.2.0 | freetype |
| bzip2 (core, tool) | 1.0.8#6 | freetype |
| zlib | 1.3.1 | freetype / libpng |
| vcpkg-cmake | 2024-04-23 | vcpkg internal tooling |
| vcpkg-cmake-config | 2024-05-23 | vcpkg internal tooling |

## Notes

- All versions are pinned by the `builtin-baseline` hash in `vcpkg.json` (`dd306f32e07d87fdb16837af64f33b6b415c770a`). They will not change on a clean build unless that baseline is updated or an explicit version override is added.
- SFML 3.0.2 was used instead of the originally planned 2.6.2 after testing showed SFML 3 compiles and runs correctly with the GCC/MinGW toolchain on this machine. imgui-sfml 3.0 was selected to match, since imgui-sfml 3.0 requires SFML 3.
- Re-run `CMake: Configure` and check the "packages are already installed" list at any time to confirm these versions are still current.
