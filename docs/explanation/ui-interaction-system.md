# UI Interaction System

## Overview

The UI is immediate-mode in structure: each frame gathers input state, updates panel interaction state, emits actions, and renders. This keeps the UI responsive and makes state transitions explicit.

## State Ownership

Main state buckets:

- `muzza_ui_state`: persistent UI/session state.
- `muzza_input_state`: per-frame mouse, wheel, keyboard modifier, and shortcut state.
- `muzza_ui_actions`: commands emitted by UI and consumed by `app.c`.

## Action Boundary

Use `muzza_ui_actions` for durable model or infrastructure operations:

- Toggle playback.
- Open/close import browser.
- Open export panel.
- Start export.
- Import path.
- Delete media.
- Insert media.
- Delete clip.
- Split clip.

Guideline: if an operation changes project structure, file IO, export lifecycle, or global app state, emit an action and let `app.c` perform it. If an operation is transient UI state or direct manipulation already scoped to a panel, keep it in the panel.

## Panels

### Media Panel

Owns source selection, context menu state, and drag state for media-to-timeline placement. Must not create decoders directly or remove clips directly when deleting media.

### Timeline Panel

Owns timeline scroll and zoom state, tool state, scrub/drag/trim interaction state, track hit testing, and clip rendering. Must preserve time-based editing semantics and project duration recalculation after timing changes.

### Monitor Panel

Owns rendering of `preview.current_frame` and aspect-fit presentation. Must not choose active decoders, advance playhead, or mutate project.

### Import and Export Panels

Own dialog-local drawing and controls, window drag affordances, and status text. Must expose actions for import selection, export start, cancel, and close.

## Dialog Windows

Current dialogs use independent SDL windows with Vulkan/Flux surfaces.

- Dialog creation/destruction belongs in `app.c`.
- Dialog panels render into their own surfaces.
- Hit testing may mark header regions draggable.
- Dialog UI scale must follow display scale and stay legible on HiDPI displays.

## Input Rules

Required shortcuts:

| Key | Action |
|---|---|
| `Space` | Toggle playback |
| `V` | Select tool |
| `C` | Razor tool |
| `Delete` | Remove selected clip |
| `Esc` | Close active dialog or quit |
| `Ctrl` + `Wheel` | Zoom timeline |
| `Wheel` | Horizontal scroll |
| `=` / `-` | Zoom in/out |

Input handling must reset one-frame press/release flags each frame, keep mouse coordinates in window-local pixel coordinates, and respect dialog focus where applicable.

## Visual Design Standards

muzza is an editing tool. The UI should be dense, scannable, and work-focused.

- Use consistent panel spacing and headers.
- Keep timeline and media controls compact.
- Keep text legible at current `ui_scale`.
- Use direct visual affordances for tools, clip states, and progress.
- Avoid decorative layouts that reduce editing surface area.
- Make disabled, active, hover, selected, and running states visually distinct.

## Further reading

- [Timeline Editing](timeline-editing.md)
- [Media Import and Bin](media-import-and-bin.md)
