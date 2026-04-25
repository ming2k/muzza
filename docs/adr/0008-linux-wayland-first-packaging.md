# ADR-0008: Linux Wayland first, Flatpak for beta

- **Status**: Accepted
- **Date**: 2024-02-15
- **Deciders**: maintainer

## Context

The project currently uses Wayland client protocols, Vulkan, SDL3, and FFmpeg. Packaging and cross-platform promises must match the actual isolation of platform-specific code.

## Decision

Focus platform support in stages:

1. Alpha: Linux Wayland source build.
2. Beta: Linux Wayland Flatpak.
3. Later: Linux X11 only if there is demand.
4. Windows/macOS only after platform-specific code is isolated behind an adapter.

## Alternatives considered

- **AppImage as default**: Rejected because Flatpak is a better fit for the SDL3 + FFmpeg + Vulkan dependency profile.
- **Promise Windows/macOS early**: Rejected because cross-platform promises should wait until the platform boundary is explicit.

## Consequences

- Positive: Alpha and beta targets are achievable with current code.
- Positive: Avoids premature abstraction for unsupported platforms.
- Trade-off: Limits early user base to Linux Wayland.
