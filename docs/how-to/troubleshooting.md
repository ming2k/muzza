# How to Troubleshoot Common Issues

## Build failures

### Missing FFmpeg development libraries

**Symptom**: `meson setup build` reports `libavformat` or related dependencies not found.

**Fix**: Install FFmpeg development packages. On Debian/Ubuntu:

```bash
sudo apt install libavformat-dev libavcodec-dev libavutil-dev libswscale-dev libswresample-dev
```

### Missing SDL3

**Symptom**: Meson cannot find SDL3.

**Fix**: Install SDL3 development packages from your distribution or build from source.

### Missing Vulkan SDK

**Symptom**: Compilation fails with Vulkan headers not found.

**Fix**: Install the Vulkan SDK or `libvulkan-dev`.

## Runtime issues

### No window appears

**Symptom**: Running `muzza` produces no window.

**Cause**: muzza requires a Wayland compositor. X11 is not supported in the current alpha.

**Fix**: Run under a Wayland session (e.g., Sway, GNOME on Wayland, KDE Plasma on Wayland).

### Import fails silently

**Symptom**: Selecting a file in the import dialog does nothing.

**Fix**: Check that the file is a supported format (MP4, WAV, PNG, JPEG, etc.). Verify the file is readable and not corrupted.

### Playback is out of sync

**Symptom**: Audio and video drift during timeline preview.

**Fix**: This is usually a decoder or performance issue. Ensure your system meets the target hardware expectations. Check that inactive decoders are being paused correctly by inspecting logs.

## Test failures

### Missing fixtures

**Symptom**: Tests fail with messages about missing media files.

**Fix**: Generate fixtures before running tests:

```bash
./scripts/generate_test_fixtures.sh
```

### One test fails after code changes

**Fix**: Run the focused test binary for faster iteration:

```bash
./build/src/muzza-core-test
./build/src/muzza-project-test
```

## Export issues

### Export fails with "invalid path"

**Fix**: Ensure the output directory exists and is writable.

### Export crashes or hangs

**Fix**: Check that you are not mutating the project structure while export is running. Cancel any active export before closing the application.

## Still stuck?

- Review the [Developer setup](../dev/setup.md) for dependency details.
- Check the [Architecture overview](../explanation/architecture-overview.md) for module boundaries.
