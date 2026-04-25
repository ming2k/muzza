# Persistence and File Format

## Overview

muzza projects are saved as `.muzza` files. The format is intentionally simple and text-based to aid debugging and version control. This document explains the format rules, versioning strategy, and why certain runtime state is excluded.

## Current Format

The project file is an INI-like text format with repeated sections:

```text
[Project]
version=1
duration=60.000000
track_height_0=40.00

[Media 0]
path=/path/to/source.mp4
duration=12.345000
has_video=1
has_audio=1
is_image=0

[Clip 0]
media=0
track=0
type=0
start=0.000000
dur=12.345000
in=0.000000
opacity=1.000
scale=1.000
pos_x=0.000
pos_y=0.000
filter=0
selected=1
```

## Persisted State

Persist:

- Project version.
- Project duration.
- Track heights.
- Media path and detected metadata.
- Clip media reference, track, type, timing, transform, filter, and current selection flag.

Do not persist:

- Decoder pointers.
- SDL windows.
- Flux/Vulkan surfaces.
- Export thread/exporter state.
- Waveform heap pointers.
- Preview frame pointers.
- Input state.

## Load Rules

Loading must:

- Create a fresh project.
- Parse sections in order.
- Reconstruct media through `project_add_media`.
- Reconstruct clips through `project_add_clip`.
- Apply optional clip transform/filter fields after clip creation.
- Recalculate project duration.
- Copy project filepath on success.

Validation:

- Media section requires non-empty path.
- Clip section requires valid `media_id`.
- Clip duration must be positive.
- Track index must be valid.
- Unknown future keys should be ignored unless required by the declared version.

## Save Rules

Saving must:

- Write complete sections.
- Use stable numeric formatting for time and transform fields.
- Write all tracks, not only modified tracks.
- Update `proj->filepath` only after successful file write.

Failure handling: if file open or write fails, return false and keep the existing project filepath. Do not partially mutate project state on save failure.

## Compatibility Policy

Version 1 is the baseline. Future versions should:

1. Prefer additive fields.
2. Increment `version` for new required fields.
3. Support older versions through defaults.
4. Keep old-field parsing for at least one major format version after removal.
5. Add stable IDs before changing reference semantics.

## Why we designed it this way

A text format makes corruption visible, diff-friendly, and easy to repair. Excluding all runtime state ensures that a saved project is a pure data snapshot: it can be loaded on a different machine or a different session without requiring the same GPU, window, or decoder state.

## Further reading

- [Project Data Model](project-data-model.md)
