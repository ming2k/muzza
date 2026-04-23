# Timeline Design

This document describes the core design logic, coordinate transformation algorithms, and interaction specifications for the Muzza timeline.

## 1. Core Coordinate System
To support large-scale projects and high-precision editing, the timeline employs an **"Absolute Time + Zoom Factor"** mapping model.

### Key Variables
- `playhead_time` (double): The absolute time of the playhead within the project (in seconds).
- `zoom` (float): The zoom factor, defined as "logical pixels per second".
- `scroll_x` (double): The absolute time offset at the left edge of the timeline view.

### Coordinate Mapping Formulas
These formulas are used for both rendering and handling mouse interactions:
- **Time to Pixel (Time to X)**:  
  `x = tracks_x + (time - scroll_x) * zoom * ui_scale`
- **Pixel to Time (X to Time)**:  
  `time = (x_coord - tracks_x) / (zoom * ui_scale) + scroll_x`

## 2. Zoom Interaction Logic
Zooming is anchored to the current mouse position to provide a natural editing experience.

### Anchor-Based Zoom Algorithm
1. Retrieve the time at the mouse cursor before zooming: `t_pre = x_to_time(mouse_x)`.
2. Apply the new `zoom` factor.
3. Retrieve the time at the mouse cursor after zooming: `t_post = x_to_time(mouse_x)`.
4. Adjust the scroll offset to keep the time point under the cursor fixed: `scroll_x += (t_pre - t_post)`.

## 3. Rendering Optimization
- **Viewport Culling**: Only clips whose `time_to_x` results fall within the range `[tracks_x, tracks_x + tracks_w]` are rendered.
- **Adaptive Ticks**: The timeline ruler tick spacing (e.g., `10s`, `5s`, `1s`, `0.1s`) adjusts dynamically based on the `zoom` level to prevent visual clutter.

## 4. Interaction Bindings
- `Ctrl` + `Wheel`: Zoom timeline centered on mouse.
- `Wheel`: Scroll timeline horizontally.
- `=` / `-`: Keyboard shortcuts for zooming.
- `Space`: Toggle playback.
