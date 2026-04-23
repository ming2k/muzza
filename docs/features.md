# Feature Roadmap

## Implemented
* **Core Editing**
    * Automatic A/V track splitting on drop.
    * Independent clip dragging and track management.
    * Clip Trimming — drag left/right edges to adjust in/out points with media-boundary constraints.
    * Razor Splitting — split any clip at the cursor position with a single click.
    * Delete selected clips with the `Delete` key.
    * Timeline Zooming and Horizontal Scrolling.
    * Multi-track real-time audio mixing support.
* **Preview System**
    * Industrial-grade master-clock AV sync engine.
    * Accurate Seeking (frame-perfect positioning).
    * Pixel-perfect real-time audio waveform rendering.
    * Source / Timeline dual-mode preview.
* **Resource Management**
    * In-app filesystem browser for imports in an independent top-level window with mouse-wheel scrolling and a visual scrollbar.
    * Static image import and placement (PNG, JPEG, GIF, WebP, BMP, TIFF, etc.) with arbitrary timeline duration.
    * Project serialization (.muzza format).
    * Dynamic track height adjustment.
* **Output**
    * Full export/render pipeline for finalized videos (MP4 with H.264 + AAC).
    * Background-thread export with live progress UI and cancel support.
    * Export completion feedback (Export button becomes "CLOSE" when done).
* **UX**
    * Toolbar with Select and Razor mode buttons.
    * Custom on-canvas tool cursors (razor blade, trim arrows) since OS cursor changes are unsupported.
    * Keyboard shortcuts `V` (Select) and `C` (Razor).
    * Independent floating Export / Import dialog windows with compositor-managed dragging.
    * HiDPI-aware dialog scaling across multiple displays.

## Planned
* **Advanced Editing**
    * Ripple edits and gap removal.
    * Basic transitions and fades.
* **Audio Enhancement**
    * Independent track volume gain.
    * Audio crossfades.
