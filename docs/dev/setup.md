# Developer Setup

This document is for contributors who will modify muzza source code.
**If you only want to use muzza**, see the [Getting Started tutorial](../tutorials/01-getting-started.md) instead.

## Requirements

- Meson 1.x+
- Ninja
- GCC or Clang with C11 support
- SDL3
- Vulkan SDK
- FFmpeg development libraries (libavformat, libavcodec, libswscale, libswresample)
- Wayland development libraries
- flux (included as Meson subproject)

## Clone and build

```bash
meson setup build
meson compile -C build
```

## Run tests

```bash
# Full test suite
meson test -C build

# Focused binaries
./build/src/muzza-core-test
./build/src/muzza-project-test
```

## Development workflow

Build incrementally with `meson compile -C build` after source changes.

## Project layout

See [project-layout.md](project-layout.md) for a tour of the source tree.

## Before submitting a PR

- [ ] All tests pass (`meson test -C build`).
- [ ] C11 style is consistent with existing code.
- [ ] `CHANGELOG.md` updated if user-visible behavior changed.
- [ ] If architectural change: ADR added.
