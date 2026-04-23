# AV Sync Engine Architecture

This document describes the design and implementation of the audio-video separation, multi-track management, and master-clock synchronization engine in Muzza.

## 1. Core Challenges
In a Non-Linear Editor (NLE), AV sync faces several critical challenges:
* **Decoupling Requirement**: Users must be able to drag audio and video tracks of the same media to different points in time.
* **Seek Precision**: Standard `av_seek_frame` only jumps to the nearest keyframe (I-Frame), causing non-accurate positioning.
* **Clock Drift**: The system main loop `delta_time` is inherently jittery, leading to playback speeds that don't match real-time.

## 2. Solution: Master-Clock Multi-Instance Design

### 2.1 Decoder Isolation (One-Decoder-Per-Clip)
To achieve complete decoupling, Muzza assigns a dedicated `muzza_decoder` instance to each `muzza_clip` on the timeline.
* Even if two clips share the same source media, they function independently in memory.
* This avoids "thrashing" where a single decoder tries to satisfy two different time positions simultaneously.

### 2.2 The Master Clock
Muzza uses the **Timeline Playhead** as the absolute Master Clock.
* The Playhead advances strictly based on the system `delta_time`.
* All active decoders act as "Followers".
* **Catch-up Mechanism**: Decoders decode as fast as possible until they reach the `target_pts` dictated by the Master Clock. If they lead the clock, they pause and wait. This eliminates the "double-speed" effect.

### 2.3 Accurate Seek
To overcome FFmpeg's keyframe limitation:
1. **Coarse Seek**: Jump to the nearest keyframe *before* the target.
2. **Buffer Flush**: Clear FFmpeg and SDL internal buffers.
3. **Decode & Discard**: Continuously decode and throw away packets until the decoder's PTS reaches the target timestamp (accurate to 1ms).

## 3. Performance Optimizations
* **Audio-Only Mode**: Decoders assigned to audio clips explicitly disable video processing pathways, significantly reducing CPU load.
* **Sync Tolerance**: Audio sync seeking is restricted to massive drifts (> 1s) to prevent audio crackling, relying on continuous streaming otherwise.
* **Waveform Pre-scanning**: Peaks are scanned once during import and cached, avoiding per-frame compute.

## 4. Future Enhancements
* **Variable Speed Preview**: By scaling the `delta_time` increment on the Master Clock, all followers will automatically adapt to 2x, 4x, or slow-motion playback.
* **Decoder Pooling**: Introduce a resource pool to reuse decoder instances when clips are removed or hidden.
