# ADR-0001: Record architecture decisions

- **Status**: Accepted
- **Date**: 2024-01-01
- **Deciders**: maintainer

## Context

muzza is a non-trivial video editor with cross-module contracts (project model, timeline semantics, playback, export, and UI). Without a durable record of why key choices were made, future contributors and agents risk silently reverting decisions or re-litigating trade-offs.

## Decision

Adopt Architecture Decision Records (ADRs) as an immutable, append-only log of significant architectural and product decisions. Each ADR is numbered sequentially, kept short, and never edited after acceptance. Superseded decisions are updated only in their `Status` line.

## Alternatives considered

- **Inline design docs**: Mutable documents lose decision history and become unreliable.
- **Commit messages only**: Difficult to discover and cross-reference.

## Consequences

- Positive: Future changes can reference prior decisions by ADR number.
- Positive: Onboarding agents and contributors can read the decision trail.
- Trade-off: Adds a small documentation burden for each major choice.
