# muzza

muzza is a high-performance non-linear video editor built on top of `flux` and `FFmpeg`, targeting Linux Wayland.

## Quick Start

```bash
meson setup build
meson compile -C build
./build/src/muzza /path/to/your/video.mp4
```

## Documentation

- [Getting Started](docs/tutorials/01-getting-started.md)
- [How-to Guides](docs/how-to/)
- [Reference](docs/reference/)
- [Architecture Overview](docs/explanation/architecture-overview.md)
- [Contributing](CONTRIBUTING.md)

## When to use this project

muzza is a good fit if you need a lightweight, frame-accurate desktop video editor with reliable playback sync and deterministic project files.

## License

MIT License — see [LICENSE](LICENSE).
