# Media Import and Bin

## Overview

The media bin is a durable inventory of source assets, not a timeline representation. This module covers filesystem browsing, media import, metadata detection, source-preview preparation, duplicate handling, and media-bin interaction.

## Current Flow

```text
Import browser UI
  -> import_browser_activate_entry
  -> app import action
  -> project_add_media
  -> project_ensure_media_decoder
  -> decoder metadata + waveform
  -> media bin selection
  -> optional source preview
```

## Import Browser Design

`muzza_import_browser_state` owns visibility, current directory, a fixed-size file entry list, selected index, scroll offset, and dialog window position/drag state.

Behavior requirements:

- Parent directory navigation must be explicit.
- Directory activation changes `current_dir` and refreshes entries.
- File activation returns an import path to the app layer.
- Scroll state must stay within visible entry bounds.

Implementation constraints:

- Do not import directly from the rendering function.
- Do not let browser state own project state.
- Keep filesystem entry caps visible and deliberate.

## Media Bin Design

The media bin presents `project->media_pool`. It owns selection and drag state through `muzza_media_panel_state`.

Required interactions:

- Select media for source preview.
- Open import browser through the add tile.
- Drag media to timeline.
- Right-click media context menu for insert/delete.

State rules:

- `selected_media_index` must be sanitized after media removal.
- Dragged media index must be invalidated if the media pool compacts.
- Deleting media must also remove dependent clips through `project_remove_media`.

## Metadata and Waveform Policy

Metadata is detected through `project_ensure_media_decoder`.

Required metadata: duration, has video, has audio, is image.

Waveform policy:

- Generate waveform peaks once per imported audio-capable media.
- Store peaks on `muzza_media.waveform`.
- Treat waveform as cache data; it is not currently serialized.
- If waveform allocation or generation fails, keep import valid and render no waveform.

## Source Preview Policy

Selecting a media item should allow source preview only when the timeline does not have an active clip at the playhead.

- Source preview uses `media.dec`.
- Timeline preview uses `clip.dec`.
- Switching selected media resets source playback session.
- Import should pause and seek the source decoder to zero after metadata detection.

## Error Handling

| Failure | Required Behavior |
|---|---|
| Empty path | Reject import. |
| Duplicate path | Return existing media ID. |
| Decoder creation failure | Remove newly added media and keep project valid. |
| Waveform failure | Keep media, omit waveform display. |
| Media deletion | Remove dependent clips and sanitize UI selection. |

## Further reading

- [Project Data Model](project-data-model.md)
- [Playback and Preview](playback-and-preview.md)
