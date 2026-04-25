# Timeline Editing

## Overview

The timeline is the primary editing surface. It translates user input into durable project mutations while keeping all editing state in seconds rather than pixels. This document explains the coordinate system, tool semantics, and invariants that keep editing predictable.

## Coordinate System

The timeline uses absolute timeline time plus a zoom factor.

Definitions:

- `playhead_time`: Absolute timeline time in seconds.
- `zoom`: Logical pixels per second before UI scale.
- `scroll_x`: Timeline time at the left edge of the visible track area.
- `ui_scale`: Display scale factor.

Mappings:

```text
x = tracks_x + (time - scroll_x) * zoom * ui_scale
time = (x - tracks_x) / (zoom * ui_scale) + scroll_x
```

Rules:

- Store editing state in seconds, not pixels.
- Convert pixels to time at the UI boundary.
- Clamp `scroll_x` to zero or greater.
- Keep cursor-anchored zoom: the timeline time under the pointer must remain stable after zoom.

### Anchor-Based Zoom

1. Retrieve the time at the mouse cursor before zooming: `t_pre = x_to_time(mouse_x)`.
2. Apply the new `zoom` factor.
3. Retrieve the time after zooming: `t_post = x_to_time(mouse_x)`.
4. Adjust scroll: `scroll_x += (t_pre - t_post)`.

## Editing Model

muzza follows a professional track-based NLE model.

Core decisions:

- Normal clips on the same track must not overlap.
- Visual compositing uses separate tracks.
- Transitions and fades must be explicit clip-boundary properties or transition objects, not arbitrary same-track overlap.
- Select-tool dragging blocks same-track overlap and snaps to valid boundaries.
- Ripple and overwrite are explicit modes/tools, not implicit behavior of normal dragging.
- AV media inserts as linked video/audio clips by default, while preserving the ability to unlink and edit them independently later.

This model gives users predictable timeline behavior and gives developers strong invariants for testing, undo/redo, save/load, preview, and export.

## Tools

### Select Tool

Responsibilities:

- Select clips.
- Drag clip bodies.
- Trim clip edges.
- Scrub playhead on ruler or empty area.
- Resize track heights.

Move behavior:

- Moving a clip must preserve `media_id`, clip type, `media_in`, duration, transform, filter, and link metadata.
- The target start time must not be negative.
- The target track must be valid.
- A move that would overlap another normal clip on the same track is invalid.
- While dragging, UI shows invalid placement state when overlap would occur.
- When near timeline zero, playhead, or clip edges, movement snaps to those boundaries.
- On release, an invalid move must either return to the original position or resolve to the nearest legal snapped position; it must not silently overlap.

Trim behavior:

- Left trim changes `tl_start`, `media_in`, and `tl_duration`.
- Right trim changes `tl_duration`.
- Non-image clips clamp to source media duration.
- Image clips may extend indefinitely because their frame can be held.
- Minimum clip duration is 100 ms for interactive trims.

### Razor Tool

- Split a clip at clicked timeline time.
- `project_split_clip` rejects split points too close to clip edges (within 50 ms).
- The right-side clip gets `media_in = original.media_in + split_offset`.
- Transform/effect fields are copied to the new clip.

## Editing Invariants

Every timeline edit must preserve:

- `tl_start >= 0`
- `tl_duration > 0`
- `media_in >= 0`
- `track_index` is valid
- `media_id` points to an existing media record
- Normal clips on the same track do not overlap
- Project duration is recalculated after timing changes
- Deleted clips release their clip decoder

## Rendering Requirements

The timeline renders track headers, clip bodies, audio waveforms, selection state, trim affordances, razor hover feedback, the playhead, and adaptive ruler ticks.

Performance requirements:

- Cull clips outside the visible viewport.
- Avoid per-frame waveform generation.
- Avoid allocations during normal timeline draw loops.

## Related decisions

- [ADR-0002: Track-based NLE timeline model](../adr/0002-track-based-nle-model.md)
- [ADR-0003: No same-track overlap](../adr/0003-no-same-track-overlap.md)
- [ADR-0004: Select-tool move behavior](../adr/0004-select-tool-move-behavior.md)
- [ADR-0005: Explicit ripple and overwrite](../adr/0005-explicit-ripple-and-overwrite.md)
- [ADR-0006: Linked AV by default](../adr/0006-linked-av-by-default.md)

## Further reading

- [Project Data Model](project-data-model.md)
- [Playback and Preview](playback-and-preview.md)
