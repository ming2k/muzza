# muzza

A professional video editor inspired by the Muses of Greek mythology. 
Built on top of the `flux` graphics library.

## Features (Planned)

- Modern, GPU-accelerated UI.
- Non-linear timeline.
- High-performance real-time preview.
- Support for professional formats via FFmpeg.

## Architecture

`muzza` uses `flux` for all its rendering needs, including the UI and the video canvas.
The goal is to keep the core of `muzza` lean and offload as much graphics work to `flux` as possible.

## Building

```bash
meson setup build
ninja -C build
```
