# Changelog

## v0.1.0 - 2026-04-23

Initial editor-shell release with a major internal refactor and the first usable end-to-end media workflow.

### Added

- Modular application structure split across `app`, `project`, `decoder`, `playback_controller`, `import_browser`, and dedicated UI panel modules.
- Custom in-app import browser instead of platform-native file dialogs.
- Project media bin with selection, source preview, context-menu delete, and drag-to-timeline insertion.
- Timeline, monitor, and media panel bitmap text rendering for readable editor chrome.
- HiDPI-aware UI scaling across the editor shell.
- Project save/load coverage and path utility coverage in automated tests.

### Changed

- Reworked preview flow around an explicit playback session instead of frame-by-frame seek chasing.
- Switched audio output setup to SDL3's device-stream model.
- Updated the timeline insertion path to use full media duration by default.
- Tightened module boundaries so future work can be developed in parallel with fewer merge conflicts.

### Fixed

- Import flow now degrades cleanly when audio output setup fails instead of rejecting otherwise-decodable media.
- Source and timeline preview state now reset more consistently across imports, playback toggles, and media removal.
- The editor no longer depends on a system file dialog for import interactions.
