# Architecture Overview

## Overview

muzza is structured into distinct layers to keep high cohesion and low coupling. The boundary between the general-purpose 2D graphics library (`flux`) and the editor (`muzza`) is strict: `flux` owns Vulkan abstraction, tessellation, and GPU resource management; `muzza` owns video/audio decoding, timeline data, UI state, and user interaction.

## Module Ownership Boundaries

### Core Playback Layer

- `src/playback_controller.[ch]`: The brain of playback. Manages the master clock, multi-track clip lookup, and decoder orchestration.
- `src/decoder.[ch]`: Hardware abstraction around FFmpeg. Provides frame-accurate seeking and audio sampling.

### Data Model Layer

- `src/project.[ch]`: State container. Defines `muzza_project`, `muzza_media`, and `muzza_clip`; manages the lifecycle of clip-specific decoders.

### Export Pipeline Layer

- `src/exporter.[ch]`: Headless renderer. Traverses the timeline, decodes source media via independent FFmpeg contexts, encodes to H.264/AAC, and muxes to MP4 on a background SDL thread.

### UI Presentation Layer

- `src/ui_timeline_panel.[ch]`: Timeline interactions and waveform visualization.
- `src/ui_media_panel.[ch]`: Media Bin management.
- `src/ui_monitor_panel.[ch]`: Preview viewport rendering.
- `src/ui_export_panel.[ch]`: Export dialog modal with live progress and cancel support.

### Infrastructure

- `src/import_browser.[ch]`: Cross-platform filesystem navigation.
- `src/ui_shared.[ch]`: Rendering primitives and visual theme constants.

## Technology Stack

| Layer | Technology | Role |
|---|---|---|
| Language | C11 | Application, project model, UI state, decoder/export orchestration. |
| Build | Meson + Ninja | Build graph, executable, tests. |
| Rendering | Flux + Vulkan | Main canvas and dialog surfaces. |
| Window/input/threading | SDL3 | Windows, events, high-DPI scaling, threads, timing. |
| Media | FFmpeg libraries | Demux, decode, encode, resample, scale, mux. |
| Platform | Wayland client protocols | Current Linux surface integration. |

## Dependency Direction

Preferred direction:

```text
app -> ui
app -> project
app -> playback_controller
app -> exporter
ui -> project
playback_controller -> project + decoder
exporter -> project + FFmpeg
project -> decoder (lazy media/clip decoder ownership)
decoder -> FFmpeg + Flux
```

Avoid reverse dependencies. `project.c` must not know about UI panels, and `exporter.c` must not know about dialog rendering.

## Key Design Patterns

1. **One-Decoder-Per-Clip**: To enable track decoupling, each clip manages its own decoder instance. See [AV Sync Engine](av-sync-engine.md).
2. **Master-Clock Sync**: A "push" synchronization model where the Playhead position dictates the target PTS for all followers.
3. **Action-Based UI**: The UI layer produces `muzza_ui_actions` which are consumed by the App layer, keeping logic out of rendering callbacks.

## Related decisions

- [ADR-0002: Track-based NLE timeline model](../adr/0002-track-based-nle-model.md)
- [ADR-0008: Linux Wayland first packaging](../adr/0008-linux-wayland-first-packaging.md)

## Further reading

- [Project Data Model](project-data-model.md)
- [Playback and Preview](playback-and-preview.md)
- [Export Pipeline](export-pipeline.md)
