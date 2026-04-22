# muzza

`muzza` is an experimental desktop video editor built on top of `flux`.
The current codebase focuses on three things:

- a Vulkan-backed editor shell with a project media module, timeline, monitor, and in-app import browser
- FFmpeg-based media decoding with SDL audio output
- a small project model that can save and load `.muzza` timeline files

## Current State

The project is still early-stage. What exists today is a functional preview-oriented editor shell, not a full nonlinear editor yet.

Implemented:

- open a media file directly from the command line
- open a saved `.muzza` project
- import audio/video media through an in-app browser panel
- delete project media from the module with a context menu
- insert selected media into the timeline
- timeline clip placement and active-clip preview
- project serialization with clip metadata and track heights
- simple docked editor UI with play/pause and scrubbing
- built-in bitmap text rendering for key editor UI panels

Not implemented yet:

- trimming, ripple edits, transitions, and compositing
- multi-clip real-time mixing
- audio-only timeline playback/editing
- export/render pipeline

## Architecture

The project is now split by ownership boundaries instead of one growing UI/app file. The goal is to make future work parallelizable across separate areas without constant merge conflicts.

The code is split into a few small modules:

- `src/main.c`: thin entry point
- `src/app.[ch]`: application bootstrap, SDL/Vulkan setup, event loop, action dispatch
- `src/playback_controller.[ch]`: preview sync and media playback coordination
- `src/project.[ch]`: timeline/project data model, serialization, media registry, clip lookup helpers
- `src/decoder.[ch]`: FFmpeg decode pipeline, GPU image upload, audio stream management
- `src/import_browser.[ch]`: reusable in-app file browser state and filesystem navigation
- `src/path_utils.[ch]`: shared path helpers for browser/app/project code
- `src/ui.[ch]`: top-level UI orchestration and frame actions
- `src/ui_shared.[ch]`: UI drawing primitives and theme constants
- `src/ui_text.[ch]`: lightweight bitmap text rendering for editor chrome
- `src/ui_media_panel.[ch]`: project media module and context menu
- `src/ui_timeline_panel.[ch]`: timeline rendering and interaction
- `src/ui_monitor_panel.[ch]`: program/source preview panel
- `src/ui_import_browser_panel.[ch]`: custom import browser modal

Best-practice boundary:

- `flux` owns rendering primitives and GPU surfaces
- `muzza` owns editor state, decoding, playback rules, and project data

Parallel development guidance:

- timeline editing work should stay inside `project.*`, `ui_timeline_panel.*`, and `playback_controller.*`
- media-bin/import work should stay inside `import_browser.*`, `ui_media_panel.*`, and `ui_import_browser_panel.*`
- application lifecycle and hotkeys should stay inside `app.*`
- rendering primitives and shared visual language should stay inside `ui_shared.*`

## Build

```bash
meson setup build
meson compile -C build
meson test -C build
```

## Run

Open a media file:

```bash
./build/src/muzza /path/to/video.mp4
```

Open a project:

```bash
./build/src/muzza /path/to/project.muzza
```

## Controls

- `Space`: play/pause
- `Esc`: quit
- Left click the `+` tile in the media module: open the in-app import browser
- Right click a media tile: open context menu and delete media
- Left click a media tile: select it for source preview
- In the import browser: click folders to enter, `..`/up to go back, and `Import` to add the selected file
- Left click timeline: scrub playhead
- Left click clip: select clip
- Drag split lines: resize panels

## Notes

- Imported media is decoded lazily when it is needed for preview.
- File importing now uses a custom application-side browser instead of the platform dialog, so interaction and styling stay consistent across platforms.
- The project media module accepts decodable audio or video assets and stores lightweight type metadata for rendering.
- The monitor can preview selected source media or the active timeline clip. Audio-only assets are managed in the media module but do not yet provide a dedicated waveform/source viewer.
- The active preview clip is chosen from the highest occupied track under the playhead.
- The app layer now consumes explicit UI actions instead of poking directly through panel-local flags, which reduces coupling between the event loop and rendering modules.
- The sample `bad-apple.mkv` in the repo is ignored by Git and can be used for local smoke testing.
