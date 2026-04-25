# ADR-0002: Track-based NLE timeline model

- **Status**: Accepted
- **Date**: 2024-01-15
- **Deciders**: maintainer

## Context

Non-linear editors support multiple timeline models: track-based (professional NLE), magnetic single-track, and node-based compositing. The model determines overlap rules, preview/export behavior, and testability.

## Decision

Use a professional track-based NLE model. Tracks are explicit editing lanes; layering uses separate tracks; same-track normal clip overlap is not allowed.

## Alternatives considered

- **Magnetic single-track**: Rejected because it hides clip identity and makes multi-track audio mixing and compositing awkward.
- **Node-based**: Rejected because it is overkill for the current editing scope and hurts predictability for basic cut/trim workflows.

## Consequences

- Positive: Users can reason about which clip is active.
- Positive: Preview/export behavior is easier to define.
- Positive: Undo/redo and save/load become more deterministic.
- Positive: Tests can enforce strong invariants.
