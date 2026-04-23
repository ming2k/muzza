# System Architecture

## Module Ownership Boundaries
The project is split into distinct layers to ensure high cohesion and low coupling:

* **Core Playback Layer**
    * `src/playback_controller.[ch]`: **The Brain**. Manages the master clock, multi-track clip lookup, and decoder orchestration.
    * `src/decoder.[ch]`: **Hardware Abstraction**. Wraps FFmpeg pipelines, providing frame-accurate seeking and audio sampling.
* **Data Model Layer**
    * `src/project.[ch]`: **State Container**. Defines Project, Media, and Clip structures; manages the lifecycle of clip-specific decoders.
* **Export Pipeline Layer**
    * `src/exporter.[ch]`: **Headless Renderer**. Traverses the timeline, decodes source media via independent FFmpeg contexts, encodes to H.264/AAC, and muxes to MP4 on a background SDL thread.
* **UI Presentation Layer**
    * `src/ui_timeline_panel.[ch]`: Timeline interactions and waveform visualization.
    * `src/ui_media_panel.[ch]`: Media Bin management.
    * `src/ui_monitor_panel.[ch]`: Preview viewport rendering.
    * `src/ui_export_panel.[ch]`: Export dialog modal with live progress and cancel support.
* **Infrastructure**
    * `src/import_browser.[ch]`: Cross-platform filesystem navigation.
    * `src/ui_shared.[ch]`: Rendering primitives and visual theme constants.

## Key Design Patterns
1. **One-Decoder-Per-Clip**: To enable track decoupling, each clip manages its own decoder instance. See [AV_SYNC_ENGINE.md](AV_SYNC_ENGINE.md).
2. **Master-Clock Sync**: A "push" synchronization model where the Playhead position dictates the target PTS for all followers.
3. **Action-Based UI**: The UI layer produces `muzza_ui_actions` which are consumed by the App layer, keeping logic out of rendering callbacks.
