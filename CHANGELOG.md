# Changelog

## v0.1.6 - 2026-04-23

Image media support, import browser polish, and flux swapchain stability fixes.

### Added

- **Image Media Support**: Static images (PNG, JPEG, GIF, WebP, BMP, TIFF, and more) can now be imported into the media bin and placed on the timeline.
- **Image Default Duration**: Images dragged to the timeline automatically receive a 5-second default duration (instead of zero).
- **Infinite Image Trimming**: Image clips can have their duration extended arbitrarily by dragging the right edge; they are not capped by media duration.
- **Image Export**: Static images are correctly repeated frame-by-frame for the full clip duration during MP4 export.
- **Image Visual Identity**: Images appear in the Media Bin with a purple "IMAGE" label and are distinguishable from video/audio assets.
- **Import Browser Scrollbar**: A visual scroll thumb now appears between the up/down arrows, showing current scroll position relative to the file list.
- **Import Browser Mouse Wheel**: Hovering over the file list and scrolling the mouse wheel now scrolls the list.

### Changed

- `decoder_is_image()` detects static images via FFmpeg codec ID heuristics (MJPEG, PNG, GIF, WebP, BMP, TIFF, etc.) with a duration-based fallback.
- `muzza_media` now carries an `is_image` boolean field; this is persisted in `.muzza` project files.
- Import dialog window now calls `SDL_RaiseWindow` on creation to ensure it receives keyboard focus immediately.
- Escape key handling is now dialog-priority: if the Import dialog is visible, Escape closes it regardless of which window holds OS focus.

### Fixed

- **Import Browser Cancel Crash**: Clicking Cancel (or the X button) now synchronously destroys the dialog window alongside hiding the browser state, eliminating a one-frame gap where stale window events could route to the main window and cause accidental application quit.
- **Swapchain Spam**: Fixed excessive `[flux I] swapchain ...` log spam caused by `dialog_window_sync_surface` calling `fx_surface_resize` every frame. `fx_surface_resize` now only marks `needs_recreate` when the requested dimensions actually change.

## v0.1.5 - 2026-04-23

Independent dialog windows, multi-surface rendering, and improved UI icons.

### Added

- **Independent Dialog Windows**: Export and Import dialogs are now true Wayland-compositor-managed top-level windows (`xdg_toplevel`), draggable outside the main window boundary.
- **SDL Hit-Test Dragging**: Borderless dialog windows use `SDL_SetWindowHitTest` with `SDL_HITTEST_DRAGGABLE` on the header bar; the compositor handles window movement.
- **Multi-Surface Rendering**: Each dialog owns an independent `SDL_Window` + `fx_surface`; the main render loop acquires and presents all visible surfaces per frame.
- **Dynamic Surface Resizing**: Dialog `fx_surface` dimensions automatically synchronize with `SDL_GetWindowSizeInPixels` via `fx_surface_resize` when moved across displays.
- **New UI Icons**: Added `draw_icon_close` (X), `draw_icon_arrow_up/down`, `draw_icon_folder`, and `draw_icon_file` to `ui_icons.h` for consistent visual language across dialogs.
- **Export Completion Feedback**: When export finishes, the Export button transforms into a "CLOSE" button with accent color to signal completion.
- **`fx_context_get_instance`**: Added to the `flux` public API to enable multi-window Vulkan surface creation from a shared context.

### Changed

- Export and Import dialogs no longer draw a full-screen modal overlay; they render as non-blocking first-class windows.
- Dialog coordinate systems switched from parent-window-relative to window-local (0,0 origin).
- `ui_handle_event` routes input by `windowID`; keyboard shortcuts (`Esc`, `Space`, `Delete`) are window-aware.

### Fixed

- Fixed dialog mouse hit-testing misalignment caused by mixing logical and pixel window sizes.
- Fixed blank dialog surfaces by ensuring `fx_surface_create_vulkan` uses actual pixel dimensions from `SDL_GetWindowSizeInPixels`.
- Fixed main window "freeze" where `ui_media_panel` and `ui_timeline_panel` incorrectly blocked interaction when a dialog was marked visible.

## v0.1.4 - 2026-04-23

Timeline editing tools: clip trimming, razor splitting, and custom cursor visuals.

### Added

- **Clip Trimming**: Drag left or right clip edges to adjust in/out points. Trim handles highlight on hover and constrain to media bounds.
- **Razor Tool**: Click the `CUT` button or press `C` to enter razor mode, then click any clip to split it at the playhead position.
- **Toolbar**: Timeline header now shows `SEL` and `CUT` buttons for switching between select and razor modes.
- **Delete Key**: Press `Delete` to remove the currently selected clip from the timeline.
- **Tool Shortcuts**: `V` switches to Select mode, `C` switches to Razor mode.
- **Custom Tool Cursors**: Razor mode displays a red blade icon at the cursor; trim hover shows directional arrows since the OS cursor cannot be changed.

### Fixed

- `reset_actions()` now properly initializes `delete_clip_index` and `split_clip_index` to `-1`.

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
