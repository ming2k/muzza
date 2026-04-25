# Milestones

## Purpose

Delivery sequence from stabilization through production release. Each milestone should finish with a working editor, passing tests, updated docs, and a short release note.

## M0: Baseline Documentation and Guardrails

Status: Complete.

Scope: design package, agent execution guide, milestone plan, task backlog, test fixture plan, release readiness checklist.

## M1: Core Model Stabilization

Status: Complete.

Outcomes: stable media and clip IDs; linked AV metadata; project-level helpers for move, trim, split; stronger project load validation; expanded regression tests.

## M2: Timeline Editing Completeness

Status: Complete.

Outcomes: no-same-track-overlap enforced; linked AV behavior; ripple delete and gap removal; track controls foundation; better edit feedback.

## M3: Playback and Export Reliability

Status: Complete.

Outcomes: export snapshot; shared render-selection semantics; missing-media behavior; export cancellation tests; fixture-driven AV sync smoke tests.

## M4: Production UX and Error Recovery

Status: Complete.

Outcomes: save/load UX; export settings UX; missing-media relink; dialog polish; keyboard shortcuts; error message standards.

## M5: Packaging and Release Candidate

Status: In progress (alpha source build ready; beta Flatpak pending).

Outcomes: Linux Wayland source build and Flatpak strategy; release smoke projects; performance baseline; known issues list.

Exit criteria:

- Clean build and tests pass from fresh checkout.
- Release artifact launches and completes import/edit/export smoke test.
- Known production risks are documented.
