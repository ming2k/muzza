# muzza

`muzza` is a high-performance non-linear video editor built on top of `flux` and `FFmpeg`.

## Core Features
- **Industrial-Grade AV Sync**: Multi-instance master-clock architecture ensuring smooth preview and scrubbing.
- **Decoupled Track Design**: Automatic A/V splitting with independent clip dragging and multi-track mixing.
- **Visual Feedback**: Real-time pixel-perfect audio waveforms and frame-accurate seeking.
- **Export & Render**: Background-thread MP4 export (H.264 + AAC) with live progress UI and cancel support.
- **Modern Architecture**: Vulkan rendering backend with a modular ownership-based model.

## Quick Start
```bash
meson setup build
meson compile -C build
./build/src/muzza /path/to/your/video.mp4
```

## Documentation
Detailed information is available in the `docs/` directory:

- [**Usage Guide**](docs/USAGE.md): UI interactions, shortcuts, and preview modes.
- [**System Architecture**](docs/ARCHITECTURE.md): Module boundaries and design patterns.
- [**AV Sync Engine**](docs/AV_SYNC_ENGINE.md): Technical deep dive into accurate seeking and clock synchronization.
- [**Feature Roadmap**](docs/FEATURES.md): Implemented capabilities and future plans.
- [**Development Guide**](docs/DEVELOPMENT.md): Build environment setup and testing standards.

## License
[MIT License]
