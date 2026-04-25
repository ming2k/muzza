# ADR-0006: Linked AV clips by default

- **Status**: Accepted
- **Date**: 2024-02-01
- **Deciders**: maintainer

## Context

When importing media with both audio and video, users typically want A/V synchronized. Independent movement is useful later, but should be an explicit unlink action.

## Decision

AV inserts create linked video/audio clips by default. Users can unlink later for independent editing.

## Alternatives considered

- **Always independent**: Rejected because it forces users to manually sync A/V on every insert.
- **Locked groups with no unlink**: Rejected because independent audio editing is a common advanced need.

## Consequences

- Positive: Default behavior matches user expectation.
- Positive: Unlink provides an escape hatch for advanced workflows.
- Trade-off: Link metadata adds a small project-model field.
