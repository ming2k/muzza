# Roadmap

## Overview

This document identifies planned modules and safe extension points. It is not a promise to implement every feature immediately; it is a guide for adding capabilities without damaging current architecture.

## Near-Term Roadmap

### Undo and Redo

Add an edit-command layer between UI actions and project mutation.

Prerequisite: stable media and clip IDs, or a carefully documented command rebasing strategy after array compaction.

### Ripple Edit and Gap Removal

Add editing operations that shift downstream clips after trim/delete.

Policy: ripple is an explicit tool or mode, not default Select-tool drag behavior. It must preserve the no-same-track-overlap invariant.

### Transitions and Fades

Add clip-boundary effects.

Recommended path: fade-in/fade-out fields first, then transition objects after fades are stable.

### Track Controls

Add track-level controls: mute, solo, gain, lock, height presets, name/color.

## Medium-Term Roadmap

### Render Graph

Introduce a render graph before adding complex compositing. Responsibilities: layer ordering, transform application, filters, opacity, transitions, and background policy. Preview and export should consume the same render graph definition.

### Media Cache

Introduce a cache for waveforms, thumbnails, video metadata, and optional proxy media. Cache files must be disposable; project files should reference source media, not require cache files.

### Export Snapshot

Create an immutable snapshot at export start. Benefits: allows editing during export, avoids data races, captures render settings deterministically, and makes export tests easier.

## Long-Term Roadmap

### Cross-Platform Windowing

Isolate platform-specific code behind an app/window adapter. Windows/macOS support should wait until the adapter exists and media/render dependencies are packaged deliberately.

### Plugin or Scripting API

Do not expose a plugin API until stable IDs, command transactions, and versioned project schema exist.

### Advanced Audio

Track gain, clip gain, pan, crossfades, loudness normalization, and a master limiter. Playback and export must use the same mix semantics.

## Safe Extension Points

| Domain | Safe Extension Point |
|---|---|
| Project data | Add fields with defaults, then update save/load and round-trip tests. |
| Timeline tools | Add a new enum value in `muzza_timeline_tool` and keep tool behavior isolated. |
| Export settings | Add render settings struct passed into exporter creation. |
| UI panels | Add panel-local state to `muzza_ui_state` and actions to `muzza_ui_actions`. |
| Playback | Add policy inside `playback_controller.c`; keep decoder API small. |
| File format | Increment version when adding required semantics. |

## Change Safety Rules

- If preview and export should match, implement shared semantics first.
- If an operation changes clips, add a project-model test.
- If a feature needs background work, define cancellation before implementation.
- If a field is serialized, define its default and migration behavior.
- If a feature uses source media paths, define missing-file behavior.
