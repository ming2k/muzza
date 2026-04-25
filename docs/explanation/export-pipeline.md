# Export Pipeline

## Overview

Export is a headless render that reads the project timeline and produces an MP4 file. It must run independently from the live preview so that users can continue editing (or at least interact with the UI) while export progresses.

## Architecture

`muzza_exporter` is a headless renderer and encoder. It owns:

- Output path.
- Export status and progress.
- Cancellation flag.
- Error message.
- Output FFmpeg muxer and encoders.
- Video frame/scaler state.
- Audio frame/mixing buffers.
- Per-source export decoder cache.

It does not own UI windows, preview decoders, or project lifetime. The app must keep the referenced project alive until export completes or is cancelled and joined.

## Export Flow

```text
export panel start
  -> exporter_create(project, output_path)
  -> exporter_start_thread
  -> export thread initializes output contexts
  -> iterate timeline time
    -> select active top video clip
    -> select active audio clips
    -> decode source media through export decoder cache
    -> composite/select video frame
    -> mix audio
    -> encode packets
    -> update progress
    -> check cancellation
  -> finalize muxer
  -> status = DONE | CANCELLED | ERROR
```

## Threading Contract

Allowed from UI/main thread:

- Create exporter before thread start.
- Start export thread.
- Poll status/progress/error.
- Request cancellation.
- Join and destroy exporter.

Allowed from export thread:

- Mutate exporter internal FFmpeg state.
- Update volatile status/progress fields.
- Read immutable project data.

Required safety rules:

- Do not mutate project structure while export is reading it unless a snapshot mechanism is implemented.
- Do not destroy exporter until its thread has joined.
- Cancellation must be cooperative and checked regularly.

## Project Snapshot Recommendation

The current exporter stores a project pointer. For stronger correctness, future work should create an immutable export snapshot containing media paths, normalized clip list, project duration, and render settings. This would allow timeline editing during export without data races.

## Video Policy

At each timeline time, choose the top active video clip by highest track index. If no active video exists, export should produce a deterministic blank frame or defined background.

## Audio Policy

- Find active audio clips at export time.
- Decode each active source segment.
- Resample to 48 kHz stereo float planar.
- Mix into an output buffer.
- Encode as AAC.

Future requirements: clip gain, pan, track mute/solo, and a master limiter.

## Status Model

| Status | Meaning |
|---|---|
| `MUZZA_EXPORT_IDLE` | Ready to start. |
| `MUZZA_EXPORT_RUNNING` | Show progress and cancel affordance. |
| `MUZZA_EXPORT_DONE` | Allow close. |
| `MUZZA_EXPORT_CANCELLED` | Allow close or restart after cleanup. |
| `MUZZA_EXPORT_ERROR` | Show error and allow close. |

## Why we designed it this way

Coupling export to preview decoders would create contention for decoder state and make cancellation cleanup fragile. A fully independent pipeline means export can be tested headlessly and preview can be paused or restarted without affecting the render.

## Further reading

- [Playback and Preview](playback-and-preview.md)
- [Project Data Model](project-data-model.md)
