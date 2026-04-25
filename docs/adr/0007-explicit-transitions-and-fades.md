# ADR-0007: Explicit transitions and fades model

- **Status**: Accepted
- **Date**: 2024-02-01
- **Deciders**: maintainer

## Context

Transitions can be implemented by allowing same-track overlap, by adding clip-boundary fade fields, or by introducing standalone transition objects. Each approach has different implications for the no-overlap invariant.

## Decision

Transitions and fades are explicit concepts. Preferred sequence:

1. Add clip fade-in/fade-out fields.
2. Apply fades consistently in playback and export.
3. Add transition objects only after fades are stable.

Do not implement transitions by permitting arbitrary same-track overlap.

## Alternatives considered

- **Overlap-as-transition**: Rejected because it violates the no-overlap invariant and complicates project-model tests.

## Consequences

- Positive: Invariants remain simple and testable.
- Positive: Preview and export semantics are identical.
- Trade-off: Transition support arrives later than a naive overlap model.
