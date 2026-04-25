# Project Layout

## Source Tree

```text
src/
  app.c / app.h                Application shell; SDL/Flux lifecycle, action dispatch
  main.c                       Entry point
  project.c / project.h        Project model; media, clips, serialization
  decoder.c / decoder.h        FFmpeg wrapper; frame-accurate seek, audio/video decode
  playback_controller.c / .h   Master clock, active clip selection, preview sync
  exporter.c / exporter.h      Background MP4 export; H.264 + AAC encode
  import_browser.c / .h        Filesystem navigation for import dialog
  path_utils.c / .h            Path helpers
  ui.c / ui.h                  UI root state, input, action generation
  ui_*_panel.c / .h            Per-panel rendering and interaction
  ui_shared.c / .h             Rendering primitives and theme constants
  ui_text.c / .h               Text rendering helpers
  ui_icons.h                   Icon glyph definitions
  test_*.c                     Regression tests
```

## Build Files

- `meson.build` — Root build definition.
- `src/meson.build` — Source and test targets.
- `subprojects/flux` — Flux Vulkan 2D graphics engine (Meson subproject).

## Documentation

- `docs/tutorials/` — Learning-oriented guides.
- `docs/how-to/` — Task-oriented recipes.
- `docs/reference/` — Lookup tables.
- `docs/explanation/` — Architecture and design rationale.
- `docs/adr/` — Immutable architecture decision records.
- `docs/dev/` — Contributor docs (this directory).

## Other

- `scripts/generate_test_fixtures.sh` — Fixture generation script.
- `test_fixtures/generated/` — Ignored; populated by the fixture script.
