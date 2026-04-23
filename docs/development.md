# Development Guide

## Language Policy
English is the official language for this project. All source code (variable names, comments), commit messages, and documentation within the `docs/` directory must be written in English to ensure accessibility for the global developer community and compatibility with automated tooling.

## Requirements
* Meson 1.x+
* Ninja
* FFmpeg development libraries (libavformat, libavcodec, libswscale, libswresample)
* SDL3
* Vulkan SDK

## Build Instructions
```bash
meson setup build
meson compile -C build
```

## Testing Standards
This project uses regression tests to ensure core logic remains stable:
* **Core Logic**: `./build/src/muzza-core-test`
* **Project Serialization**: `./build/src/muzza-project-test`

Run all tests:
```bash
meson test -C build
```

## Coding Style
* Standards: C11.
* No blocking I/O inside the decoder loops.
* Memory Management: Resources created in `project_add_clip` MUST be freed in `project_remove_clip`.
