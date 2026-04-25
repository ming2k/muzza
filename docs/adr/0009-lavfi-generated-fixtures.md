# ADR-0009: FFmpeg lavfi generated test fixtures

- **Status**: Accepted
- **Date**: 2024-03-01
- **Deciders**: maintainer

## Context

Automated tests for playback and export require deterministic media. Committing large binary files bloats the repository and raises license concerns.

## Decision

Generate media fixtures with FFmpeg lavfi scripts. Do not commit large binary media files. Generated outputs live under an ignored path such as `test_fixtures/generated/`. Tests that require fixtures fail with a clear generation instruction when fixtures are missing.

## Alternatives considered

- **Commit binary fixtures**: Rejected because it bloats the repo and complicates licensing.
- **Download fixtures at test time**: Rejected because it introduces network dependency and supply-chain risk.

## Consequences

- Positive: Repository stays small.
- Positive: Fixtures are deterministic and reproducible.
- Positive: CI can generate fixtures on demand.
- Trade-off: First test run requires FFmpeg and a generation step.
