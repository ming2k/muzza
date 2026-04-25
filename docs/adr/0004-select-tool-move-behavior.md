# ADR-0004: Select-tool movement blocks overlap and snaps

- **Status**: Accepted
- **Date**: 2024-01-20
- **Deciders**: maintainer

## Context

Drag-and-drop in a timeline can either allow free placement (with later resolution) or enforce constraints during the drag. Free placement leads to silent overlap or unexpected ripple/overwrite side effects.

## Decision

Select-tool movement blocks same-track overlap and snaps to legal boundaries. Illegal placement shows invalid feedback; releasing on an invalid position must not silently create overlap.

## Alternatives considered

- **Free placement with auto-resolve**: Rejected because auto-resolve behavior is hard to predict and test.
- **Implicit ripple on drag**: Rejected because ripple is an explicit editing intent, not a default drag behavior.

## Consequences

- Positive: Users learn the editing invariant immediately.
- Positive: No accidental ripple or overwrite.
- Trade-off: Dragging requires slightly more precision.
