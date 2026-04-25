# ADR-0005: Ripple and overwrite are explicit modes

- **Status**: Accepted
- **Date**: 2024-01-20
- **Deciders**: maintainer

## Context

Ripple and overwrite are powerful editing operations, but making them implicit side effects of normal dragging creates data loss risk and confusing UX.

## Decision

Ripple and overwrite are explicit modes or tools, not default Select-tool behavior.

- Ripple shifts downstream clips intentionally.
- Overwrite replaces occupied time intentionally.

## Alternatives considered

- **Ripple as default drag behavior**: Rejected because it shifts content the user did not intend to move.
- **Overwrite as default drop behavior**: Rejected because it silently removes or splits existing clips.

## Consequences

- Positive: Normal drag is safe and predictable.
- Positive: Advanced workflows are opt-in.
- Trade-off: Requires additional UI surface for mode selection.
