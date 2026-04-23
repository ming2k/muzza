# Changelog

## v0.1.3 - 2026-04-23

Full export/render pipeline with background-thread encoding, live progress UI, and independent video/audio decoder isolation.

### Added

- **Export/Render Pipeline**: Implemented `src/exporter.[ch]` providing headless FFmpeg-based MP4 export (H.264 video + AAC audio) with timeline traversal, frame composition, and multi-track audio mixing.
- **Background Thread Export**: Export runs on a dedicated SDL thread so the UI remains fully responsive with a smooth 60 FPS progress bar.
- **Export Dialog UI**: Added `src/ui_export_panel.[ch]` featuring a modal overlay with live progress bar, status messages, and Cancel support.
- **Export Trigger**: New "EXPORT" button in the top header bar opens the export dialog.
- **Audio Mixing**: Simple additive audio mixing with per-track gain attenuation (`1/sqrt(n)`) and soft-limiting saturation for clean multi-track output.
- **Independent Decoder Instances**: Video and audio export decoders now open separate file handles to prevent cross-stream packet loss.

### Fixed

- Eliminated H.264 "reference picture missing" decode errors caused by shared `AVFormatContext` dropping interleaved packets.
- Fixed audio silence in exported files caused by `src_dec_refill_audio` never executing due to a zero-length buffer target.
- Fixed video stuttering (keyframes-only playback) by reducing seek frequency and decoding forward to the exact target PTS.

## v0.1.2 - 2026-04-23

Added Timeline Zooming and Horizontal Scrolling for improved navigation in large projects.

### Added

- **Timeline Zooming**: Implemented high-precision zooming centered on the mouse cursor using `Ctrl` + `Mouse Wheel` or keyboard shortcuts (`=` and `-`).
- **Horizontal Scrolling**: Added support for horizontal timeline navigation using the `Mouse Wheel`.
- **Adaptive Time Ruler**: The timeline ruler now dynamically adjusts its tick frequency (from 10s down to 0.1s) based on the current zoom level.
- **Precision Playhead**: Upgraded the internal playhead from a normalized percentage to an absolute double-precision time value, enabling frame-perfect accuracy at high zoom levels.
- **Visual Improvements**: Audio waveforms and clip labels now scale smoothly with zoom, and off-screen clips are automatically culled during rendering for better performance.

## v0.1.1 - 2026-04-23

Industrial-grade playback engine refactor with independent track management, accurate seeking, and high-fidelity audio visualization.

### Added

- **Master-Clock Sync Engine**: Implemented a global timeline clock architecture for perfectly stable 1.0x playback and eliminated frame thrashed during multi-track playback.
- **Accurate Seeking**: Introduced a "decode-and-discard" strategy to achieve frame-perfect positioning, overcoming the limitations of coarse keyframe-only seeking.
- **Automatic A/V Splitting**: Dropping media into the timeline now automatically creates separate, independently editable video and audio clips.
- **Audio Waveforms**: Added smooth, pixel-perfect loudness waveforms for all audio clips, featuring automatic track-height scaling and peak normalization.
- **Independent Track Editing**: Implemented drag-and-drop support for moving individual clips across time and tracks.
- **Physical Stream Isolation**: Decoders now support dedicated Video/Audio-only modes, physically disconnecting unused hardware streams to eliminate noise and reduce CPU load.
- **Core Regression Tests**: Added `muzza-core-test` to verify AV separation, multi-instance decoder allocation, and project model integrity.
- **Comprehensive Documentation**: Established a dedicated `docs/` directory covering Architecture, Sync Engine, Usage, and Development guides.

### Changed

- Switched to global time-base seeking (using `-1` stream index) to improve compatibility with Matroska (MKV) and WebM containers.
- Standardized all documentation to English.
- Refactored playback controller to a "Follower" model where decoders synchronize to the master timeline clock.

### Fixed

- Fixed a bug where video playback was unintentionally accelerated by a 1.25x hardcoded factor.
- Eliminated audio crackling and "jitter" during scrubbing by loosening sync tolerance and using precise buffer flushes.
- Resolved "File is broken" warnings in MKV files by aligning seek targets to global timestamps.

## v0.1.0 - 2026-04-23

Initial editor-shell release with a major internal refactor and the first usable end-to-end media workflow.

### Added

- Modular application structure split across `app`, `project`, `decoder`, `playback_controller`, `import_browser`, and dedicated UI panel modules.
- Custom in-app import browser instead of platform-native file dialogs.
- Project media bin with selection, source preview, context-menu delete, and drag-to-timeline insertion.
- Timeline, monitor, and media panel bitmap text rendering for readable editor chrome.
- HiDPI-aware UI scaling across the editor shell.
- Project save/load coverage and path utility coverage in automated tests.

### Changed

- Reworked preview flow around an explicit playback session instead of frame-by-frame seek chasing.
- Switched audio output setup to SDL3's device-stream model.
- Updated the timeline insertion path to use full media duration by default.
- Tightened module boundaries so future work can be developed in parallel with fewer merge conflicts.

### Fixed

- Import flow now degrades cleanly when audio output setup fails instead of rejecting otherwise-decodable media.
- Source and timeline preview state now reset more consistently across imports, playback toggles, and media removal.
- The editor no longer depends on a system file dialog for import interactions.
