# Release Process

## Purpose

This checklist is the final gate before publishing a production or release-candidate build. Every item must be completed, explicitly waived, or converted into a known issue.

## Version and Scope

Current release target: `0.2.0-alpha`

Target platform: Linux Wayland source build (alpha); Linux Wayland Flatpak (beta).

Supported workflow: Import -> edit -> preview -> save/load -> export.

## Build Gate

- [ ] Clean checkout builds with documented commands.
- [ ] `meson setup build` succeeds.
- [ ] `meson compile -C build` succeeds.
- [ ] `meson test -C build` passes.
- [ ] No generated build artifacts are accidentally tracked.
- [ ] Dependency versions are documented.

## Automated Test Gate

- [ ] Project model tests pass.
- [ ] Core regression tests pass.
- [ ] Save/load round-trip tests pass.
- [ ] Invalid project file tests pass.
- [ ] Export cancellation/error tests pass.
- [ ] Fixture-based playback/export tests pass.

## Manual Product Gate

- [ ] Import supported video, audio, AV, and image files.
- [ ] Source preview works.
- [ ] Timeline preview works.
- [ ] AV split insert works.
- [ ] Move, trim, split, delete work.
- [ ] Track resize works and persists.
- [ ] Save project works.
- [ ] Load project works.
- [ ] Missing-media behavior is understandable.
- [ ] Export completes and output plays externally.
- [ ] Export cancellation works.
- [ ] Import/export dialogs behave correctly with Esc and close actions.

## Performance Gate

- [ ] Performance target hardware is documented.
- [ ] Launch time recorded.
- [ ] Import time recorded.
- [ ] Timeline interaction responsiveness checked with stress project.
- [ ] Playback drift checked.
- [ ] Export time recorded.
- [ ] Export cancellation latency recorded.

## UX and Error Recovery Gate

- [ ] User-facing errors do not require terminal logs.
- [ ] Failed import does not create broken project state.
- [ ] Failed save preserves previous project state.
- [ ] Failed load cleans up partial allocations.
- [ ] Failed export reports error and allows recovery.
- [ ] Missing media can be identified by the user.
- [ ] Dialogs are readable at target display scale.

## Documentation Gate

- [ ] README quick start is accurate.
- [ ] Usage guide reflects current shortcuts and workflows.
- [ ] Design docs reflect changed architecture and semantics.
- [ ] File format docs reflect current version.
- [ ] Known limitations are documented.
- [ ] Release notes include new features, fixes, limitations, and migration notes.

## Packaging Gate

- [ ] Alpha source-build instructions tested on Linux Wayland.
- [ ] Flatpak plan documented for beta.
- [ ] Build artifact launches outside the build tree.
- [ ] Runtime dependencies documented.
- [ ] Sample project and fixture media instructions included.
- [ ] License file included.

## Risk Signoff

- Known issues documented.
- Waived checklist items documented.
- Release owner named.
- Maintainer approval recorded.
