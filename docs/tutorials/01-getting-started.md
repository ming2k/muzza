# Getting Started: Your First muzza Edit

By the end of this tutorial you will have:

- A running local build of muzza.
- Imported media and placed it on the timeline.
- Trimmed and split a clip.
- Exported a short MP4 video.
- Run the smoke tests to verify your environment.

**Estimated time:** 15 minutes
**Difficulty:** Beginner

## Prerequisites

- Linux with Wayland
- Meson 1.x+, Ninja, GCC or Clang with C11 support
- SDL3, Vulkan SDK, FFmpeg development libraries
- A sample video file (any MP4)

## Step 1: Build muzza

```bash
meson setup build
meson compile -C build
```

You should see compilation complete without errors.

## Step 2: Launch with a video

```bash
./build/src/muzza /path/to/your/video.mp4
```

The muzza window should open with your video visible in the monitor and the media bin.

## Step 3: Import and place media

1. Click the `+` tile in the Media Bin to open the Import dialog.
2. Select a video file.
3. Drag the imported media from the Media Bin onto a timeline track.

You should see the video and audio split into separate clips on adjacent tracks.

## Step 4: Edit the timeline

1. **Move**: Left-click and drag a clip body to reposition it.
2. **Trim**: Hover over the left or right edge of a clip and drag to adjust the in/out points.
3. **Split**: Press `C` to switch to Razor mode, then click a clip to split it.
4. **Delete**: Select a clip and press `Delete` to remove it.

## Step 5: Export your project

1. Click the `EXPORT` button in the top header.
2. Confirm the output path and start export.
3. Watch the progress bar; you can cancel if needed.

When the export completes, play the output MP4 in your preferred player to verify.

## Step 6: Run the smoke tests

```bash
meson test -C build
```

All tests should pass. This verifies your environment is correctly configured.

## What's next?

- Want to understand the architecture? See [Architecture overview](../explanation/architecture-overview.md)
- Want to contribute code? See [Developer setup](../dev/setup.md)
- Need keyboard shortcuts? See [Keyboard shortcuts](../reference/keyboard-shortcuts.md)

## Troubleshooting

- **Build fails with missing FFmpeg**: Install `libavformat-dev`, `libavcodec-dev`, `libswscale-dev`, and `libswresample-dev`.
- **No window appears**: Verify you are running under a Wayland compositor.
- **Tests fail with missing fixtures**: Run `./scripts/generate_test_fixtures.sh` first.
