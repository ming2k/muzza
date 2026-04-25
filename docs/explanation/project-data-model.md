# Project Data Model

## Overview

The project model is the durable editing truth. It must remain valid if all UI state is discarded and reconstructed. All timeline edits, save/load behavior, and export semantics depend on the contracts defined here.

## Canonical Structures

### `muzza_project`

Owns:

- Project filepath.
- Dynamic media pool.
- Dynamic clip array.
- Timeline duration.
- Track heights for `MUZZA_MAX_TRACKS`.

Rules:

- `duration` is derived from clips with a minimum baseline of `MUZZA_DEFAULT_DURATION`.
- Track heights are durable project data and are serialized.
- The model must remain valid after UI state is reset.

### `muzza_media`

Represents a source file.

Fields:

- `id`: Stable identifier.
- `filepath`: Source path.
- `dec`: Lazy source-preview decoder.
- `duration`, `has_video`, `has_audio`, `is_image`: Detected metadata.
- `waveform`: Cached min/max amplitude envelope (one entry per peak bucket) used by the timeline waveform renderer.

Rules:

- Duplicate path import returns the existing media ID.
- Source decoder is for source preview and metadata; timeline clips must use clip decoders.

### `muzza_clip`

Represents a timeline segment.

Fields:

- `media_id`: Reference to source media.
- `track_index`: Track row.
- `type`: `MUZZA_CLIP_VIDEO` or `MUZZA_CLIP_AUDIO`.
- `tl_start`, `tl_duration`: Timeline placement in seconds.
- `media_in`: Source offset in seconds.
- `opacity`, `scale`, `pos_x`, `pos_y`, `filter`: Current transform/effect fields.
- `selected`: Persistent selection flag used by current UI.
- `dec`: Lazy clip-specific decoder.

Rules:

- Video and audio clips from the same media are separate clips.
- A clip owns its decoder and must destroy it when the clip is removed.
- Clip duration must be positive.
- Clip media time is `media_in + (timeline_time - tl_start)` and clamps to media duration for non-image media.

## Memory Ownership

```text
muzza_project
  owns media_pool array
    owns each media.dec
    owns each media.waveform.mins / .maxs
  owns clips array
    owns each clip.dec
```

No UI panel, playback function, or exporter should destroy project-owned decoders directly. They should call project APIs or let project destruction perform cleanup.

## Mutation API Contracts

| Function | Contract |
|---|---|
| `project_add_media` | Add or return media by path; no decoder is required at this stage. |
| `project_remove_media` | Destroy media decoder, remove dependent clips, compact media IDs. |
| `project_ensure_media_decoder` | Lazily create source decoder, detect metadata, generate waveform if audio exists. |
| `project_add_clip` | Validate media, track, duration; initialize defaults; recalculate duration. |
| `project_remove_clip` | Destroy clip decoder, compact clip array, recalculate duration. |
| `project_split_clip` | Split one clip into two adjacent clips, preserving properties and setting right-side `media_in`. |
| `project_clear_selection` | Clear clip selection flags. |
| `project_recalculate_duration` | Derive duration from clip ends and baseline duration. |

## Current Limitations

- Selection is stored on clips, which mixes UI state into the model. This is acceptable for the current baseline, but undo/redo should move selection into an edit/session layer.
- Same-track overlap is a global editing invariant for normal clips. Existing code does not fully enforce it yet, so move, insert, trim, ripple, and overwrite work must be routed through project helpers that enforce the policy.
- There is no transaction or undo stack yet.

## Related decisions

- [ADR-0003: No same-track overlap](../adr/0003-no-same-track-overlap.md)
- [ADR-0006: Linked AV clips by default](../adr/0006-linked-av-by-default.md)

## Further reading

- [Timeline Editing](timeline-editing.md)
- [Persistence and File Format](persistence-and-file-format.md)
