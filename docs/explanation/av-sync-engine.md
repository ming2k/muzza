# AV Sync Engine

## Overview

In a Non-Linear Editor (NLE), audio-video synchronization faces three critical challenges: users must be able to drag audio and video tracks of the same media to different points in time; standard `av_seek_frame` only jumps to the nearest keyframe, causing inaccurate positioning; and the system main-loop `delta_time` is inherently jittery, leading to playback speeds that do not match real time.

muzza solves these with a **master-clock multi-instance design**.

## How it works

### Decoder Isolation (One-Decoder-Per-Clip)

To achieve complete decoupling, muzza assigns a dedicated `muzza_decoder` instance to each `muzza_clip` on the timeline.

- Even if two clips share the same source media, they function independently in memory.
- This avoids "thrashing" where a single decoder tries to satisfy two different time positions simultaneously.

### The Master Clock

The **Timeline Playhead** is the absolute Master Clock.

- The Playhead advances strictly based on system `delta_time`.
- All active decoders act as "followers".
- **Catch-up Mechanism**: Decoders decode as fast as possible until they reach the `target_pts` dictated by the Master Clock. If they lead the clock, they pause and wait. This eliminates the "double-speed" effect.

### Accurate Seek

To overcome FFmpeg's keyframe limitation:

1. **Coarse Seek**: Jump to the nearest keyframe *before* the target.
2. **Buffer Flush**: Clear FFmpeg and SDL internal buffers.
3. **Silent Decode**: Decode forward to the target. Audio frames produced during this catch-up are discarded (`drop_audio_output`) instead of being queued to SDL, so playback resumes at the requested timestamp rather than at the keyframe behind it.

## Why we designed it this way

A shared decoder model would force all clips that reference the same media file to share a single playback position. In an editor where audio and video of the same interview might be minutes apart on the timeline, that model collapses immediately. The master-clock approach also isolates clock jitter in one place (the playhead) rather than letting it propagate into each decoder independently.

## Drift Detection

Audio decoders run ~250 ms ahead of the playhead so the SDL output queue stays full. That makes the decoder's last-decoded PTS unsuitable as a sync reference — it sits a quarter second past the playhead by design. Drift is therefore measured against the *playing* position:

```
play_time = decoder.current_pts − queued_seconds(SDL audio stream)
drift     = |play_time − target_media_time|
```

A re-seek only fires when `drift > 0.15 s`, when the user is scrubbing, or when the playback session is invalidated (clip switch, new session). In steady-state playback `play_time` tracks the playhead within milliseconds and no spurious seeks occur.

## Performance Optimizations

- **Audio-Only Mode**: Decoders assigned to audio clips explicitly disable video processing pathways, significantly reducing CPU load.
- **Lookahead Queue**: Each decoder maintains ~250 ms of decoded audio in the SDL stream so brief decode stalls do not glitch playback.
- **Waveform Pre-scanning**: A min/max amplitude envelope is scanned once during import and cached on `muzza_media`, avoiding per-frame compute.

## Related decisions

- [ADR-0006: Linked AV clips by default](../adr/0006-linked-av-by-default.md)

## Further reading

- [Playback and Preview](playback-and-preview.md)
- [Export Pipeline](export-pipeline.md)
