# Testing

## Build and Test Commands

```bash
meson setup build
meson compile -C build
meson test -C build
```

Focused test binaries:

```bash
./build/src/muzza-core-test
./build/src/muzza-project-test
```

## Testing Strategy

### Project Model Tests

Required for:

- Media add/remove.
- Clip add/remove.
- Split behavior.
- Trim helper behavior.
- Duration recalculation.
- Save/load round trips.
- Media ID compaction.

### Playback Tests

Use deterministic tests where possible. For FFmpeg-dependent integration, prefer small fixture media and explicit tolerance ranges.

Required scenarios:

- Clip media-time mapping.
- Source/timeline session reset.
- Decoder pause policy for inactive clips.
- Seeking on scrub/session change.

### Export Tests

Required scenarios:

- Exporter create/destroy without starting.
- Cancellation before completion.
- Error reporting for invalid output path.
- Export of simple video-only, audio-only, and AV projects when fixture media is available.

### UI Tests

Core UI interaction math should be testable without rendering:

- Time-to-X and X-to-time round trips.
- Cursor-anchored zoom.
- Track hit testing.
- Trim clamp logic.

## Test Fixtures

Fixtures are generated through FFmpeg lavfi scripts. Do not commit large binary media files.

```bash
./scripts/generate_test_fixtures.sh
```

Generated outputs live under `test_fixtures/generated/`. Tests that require fixtures should fail with a clear generation instruction when fixtures are missing.

### Required Fixture Set

| Fixture | Purpose | Properties |
|---|---|---|
| `video_only_5s.mp4` | Video decode, trim, export | 5 s, H.264, 1280x720 or smaller, 30 FPS, no audio |
| `audio_only_5s.wav` | Audio import, waveform | 5 s, 48 kHz stereo PCM |
| `av_10s.mp4` | Normal import/edit/export | 10 s, H.264 + AAC, 30 FPS, 48 kHz stereo |
| `av_44100hz.mp4` | Resampling path | AAC or PCM at 44.1 kHz |
| `short_1s.mp4` | Edge-case trimming/split | 1 s AV |
| `still.png` | Image import | Small PNG |
| `missing_source.muzza` | Missing-media load | Project with non-existent file reference |

## Manual Smoke Test Matrix

Run before a release candidate:

1. Launch empty project.
2. Import AV file, audio-only file, video-only file, and still image.
3. Preview each source in the monitor.
4. Insert AV file and confirm video/audio split.
5. Move audio clip independently.
6. Trim left and right edges.
7. Split with Razor tool.
8. Delete a selected clip.
9. Resize tracks and save project.
10. Reload project and verify timeline state.
11. Export MP4 and play output externally.
12. Start export again and cancel mid-run.
13. Load project with missing media and verify visible error state.

## Performance Budgets

Initial budgets; revise after baseline measurements on target hardware.

| Scenario | Budget |
|---|---|
| Launch empty project | Under 1 s on target hardware |
| Import 10-second 720p AV | Under 2 s including metadata and waveform |
| Timeline scroll/zoom with 100 clips | No visible stalls; target 60 FPS UI |
| Source preview seek | Frame visible within 150 ms |
| Timeline scrub | Frame feedback within 150 ms |
| Playback AV sync drift | No visible drift during 60-second playback |
| Export cancellation | UI reflects cancellation within 500 ms |
