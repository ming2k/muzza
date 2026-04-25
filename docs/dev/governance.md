# Autonomous Governance

## Purpose

This document defines how employees and coding agents may advance muzza with minimal day-to-day supervision. It exists to prevent architecture drift, silent quality regressions, and ambiguous ownership while allowing autonomous delivery.

## Authority Model

### Maintainer

The maintainer owns product direction and final release approval.

Maintainer approval is required for:

- Changing timeline semantics.
- Changing same-track overlap policy.
- Changing linked AV policy.
- Adding new third-party dependencies.
- Changing target platform or packaging strategy.
- Breaking project file compatibility.
- Removing documented user-facing behavior.
- Publishing a release artifact.

### Implementer

An implementer may be a human developer or coding agent.

Implementers may autonomously:

- Pick up `ready` backlog tasks.
- Refactor within a module when required by the task.
- Add focused helpers and tests.
- Update docs for changed behavior.
- Fix bugs discovered while working in the same module.

Implementers may not:

- Redefine product decisions.
- Skip required tests.
- Merge broad unrelated refactors.
- Introduce hidden global state.
- Make preview and export semantics diverge without documentation and tests.

### Reviewer

If a separate reviewer exists, they verify scope, tests, docs, module boundaries, user workflow, and release risk. If no reviewer exists, the implementer must perform a self-review checklist and include it in the delivery report.

## Work Intake Rules

Every autonomous task must reference one of:

- A task ID from the [backlog](backlog.md).
- A bug report with reproduction steps.
- A maintainer-approved written request.

Do not work from vague prompts such as "make it production-grade" or "improve the UI." Convert broad work into milestone tasks first.

## Branch and Change Scope

One change should contain one logical unit: one backlog task, one bug fix, one documentation update, or one test infrastructure improvement.

Allowed coupling: code + tests + docs for the same behavior; small helper extraction required by the task.

Disallowed coupling: formatting unrelated files; changing build system while implementing UI behavior; rewriting modules for style; mixing feature work with platform packaging unless required.

## Merge Gates

### Universal Gates

- Build succeeds.
- Relevant tests pass.
- Docs updated if behavior, architecture, file format, or workflow changed.
- No known memory/resource cleanup path was weakened.
- No unrelated user changes were reverted.
- Delivery report is included.

### Project Model Gate

- Project-model tests pass.
- Save/load compatibility is checked.
- Invalid input paths are considered.
- Ownership and cleanup rules remain explicit.

### Timeline Gate

- No-same-track-overlap policy is preserved.
- Select-tool move does not ripple or overwrite.
- Linked AV behavior is preserved or intentionally updated.
- Coordinate math is verified.
- Manual insert/move/trim/split/delete smoke test when UI behavior changes.

### Playback and Export Gate

- Preview and export selection semantics remain aligned.
- Inactive decoder pause policy is preserved.
- Export cancellation still has a cleanup path.
- Fixture-based tests or manual fixture smoke test when media behavior changes.

### File Format Gate

- File version behavior is documented.
- Old files still load or migration behavior is documented.
- New files round-trip.
- Invalid files fail safely.

## Stop Conditions

Implementers must stop and escalate before continuing if any condition is met:

- A task requires changing a settled product decision.
- A task requires a new third-party dependency.
- A task would break existing project files.
- A task would make preview and export intentionally different.
- A task exposes a data race or lifetime issue that cannot be fixed locally.
- Required tests cannot be run and no equivalent verification is available.
- The implementation requires broad rewrite outside the assigned module.
- The feature cannot be made compatible with no-same-track-overlap policy.
- The task affects packaging or release platform outside the Linux Wayland plan.

## Delivery Report Format

```text
Task:
Goal:
Summary:
Files changed:
Behavior changed:
Tests run:
Manual verification:
Docs updated:
Risks:
Follow-up tasks:
```
