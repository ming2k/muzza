# ADR-0003: No same-track overlap for normal clips

- **Status**: Accepted
- **Date**: 2024-01-15
- **Deciders**: maintainer

## Context

Same-track overlap can be interpreted as an implicit transition or as hidden state. This ambiguity creates unpredictable UX and complicates testing.

## Decision

Normal clips on the same track must not overlap. Overlap is reserved for explicit future concepts: transition objects, clip-boundary fade fields, or separate tracks for visual stacking.

## Alternatives considered

- **Allow arbitrary overlap**: Rejected because it creates hidden state and undefined preview/export behavior.
- **Implicit transitions via overlap**: Rejected because it conflates two concepts and makes editing invariants hard to test.

## Consequences

- Positive: Timeline behavior is predictable.
- Positive: Strong invariants simplify project-model tests.
- Trade-off: Users must use separate tracks or explicit transitions for layered visuals.
