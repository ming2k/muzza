# Task Backlog

## Purpose

Reviewable implementation tasks with scope, files, tests, and acceptance criteria.

Status values: `ready`, `blocked`, `future`.

## M5: Packaging and Release Candidate

### T021: Define Target Platform and Packaging Strategy

Status: ready

Goal: Document and implement the initial release packaging path.

Acceptance criteria:

- Alpha target is Linux Wayland source build.
- Beta packaging target is Linux Wayland Flatpak.
- AppImage is not the default path.
- Windows/macOS are out of scope until a platform adapter exists.
- Runtime dependency documentation covers SDL3, Vulkan, FFmpeg, Flux, and Wayland requirements.

### T022: Add Release Sample Projects and Fixture Manifest

Status: ready after T014

Goal: Provide deterministic release verification assets.

Acceptance criteria:

- Sample project exercises import, trim, split, playback, save/load, and export.
- Fixture manifest documents license and expected media properties.

### T023: Run Performance Baseline

Status: ready after T014

Goal: Record measurable performance targets.

Acceptance criteria:

- Measure import, timeline draw, playback, and export on defined hardware.
- Record results in testing docs or a linked report.

### T024: Complete Release Readiness Checklist

Status: ready after M4

Goal: Pass all release gates.

Acceptance criteria:

- All checklist items in [release-process.md](release-process.md) are complete or explicitly waived.
