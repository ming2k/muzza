# Playback and Preview

## Overview

muzza supports two preview modes: source preview (preview a media file from the bin) and timeline preview (preview the composed timeline). Both are governed by the same master-clock principle: the timeline playhead is the absolute time source, and decoders are followers.

## Decoder Types

| Decoder | Owner | Purpose |
|---|---|---|
| Media decoder | `muzza_media.dec` | Source preview and metadata detection. |
| Clip decoder | `muzza_clip.dec` | Timeline playback for a specific clip instance. |
| Export decoder | `exporter.c` source cache | Headless render/export only. |

Decoders must not be shared across these roles.

## Timeline Preview Flow

```text
playback_sync_preview
  -> update_timeline_preview
    -> advance playhead when playing
    -> find top active video clip
    -> find all active audio clips
    -> pause inactive decoders
    -> sync video decoder to target media time
    -> sync audio decoders to target media time
    -> publish preview.current_frame
```

Video selection: the top active video clip is the active video clip with the highest track index at the playhead time.

Audio selection: all active audio clips at the playhead are eligible, up to the current fixed active array limit.

## Source Preview Flow

Source preview is the fallback when no timeline clip is active.

- Source preview uses `media.dec`.
- Timeline preview uses `clip.dec`.
- Switching selected media resets source playback session.
- Import should pause and seek the source decoder to zero after metadata detection.
- Source preview must not advance timeline playhead time or use clip decoders.

## Sync Policy

Video sync:

- Seek on scrubbing.
- Seek when playback session changes.
- Seek when video drift exceeds 500 ms.
- Otherwise decode forward to target media time.

Audio sync:

- Seek on scrubbing.
- Seek when playback session changes.
- Seek when audio drift exceeds 1 second.
- Avoid frequent audio seeking during normal playback to reduce crackle.

Inactive decoder policy:

- Pause all media decoders except the active source decoder.
- Pause all clip decoders except the active video clip and active audio clips.

## Why we designed it this way

Decoupling the master clock from the decoders makes the system resilient to decoder stalls, seek latency, and multi-track mixing. If each decoder tried to maintain its own real-time pace, drift would compound across tracks. By making the playhead the single source of truth, all followers naturally align.

## Related decisions

- [ADR-0002: Track-based NLE timeline model](../adr/0002-track-based-nle-model.md)

## Further reading

- [AV Sync Engine](av-sync-engine.md)
- [Export Pipeline](export-pipeline.md)
