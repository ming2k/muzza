# Agent Execution Guide

## Purpose

This guide defines how coding agents should use the design and dev docs to implement muzza with minimal direct supervision.

## Required Reading Order

1. [README.md](../../README.md)
2. [Setup](setup.md)
3. [Code Style](code-style.md)
4. [Testing](testing.md)

Then read the module document that owns the task in [Explanation](../explanation/). Cross-module work must read every affected module document.

## Task Intake Template

Every task should begin with:

```text
Goal:
Affected modules:
User workflow:
Current behavior:
Target behavior:
Out of scope:
Files likely to change:
Tests required:
Manual verification:
Risks:
```

## Execution Protocol

1. Inspect existing implementation before proposing changes.
2. Identify the module owner and preserve dependency direction.
3. Keep the change scoped to one user-visible behavior or one infrastructure improvement.
4. Add or update tests for project-model and non-UI logic.
5. Update relevant docs when behavior, file format, ownership, or workflow changes.
6. Run the smallest relevant verification first, then broader tests.
7. Summarize changed files, behavior, tests, and residual risk.

## Definition of Done

A task is done only when:

- The user workflow works end to end.
- Build succeeds.
- Relevant automated tests pass.
- New failure paths clean up owned resources.
- Project save/load behavior is preserved or intentionally migrated.
- Preview and export behavior are consistent when the feature affects rendering semantics.
- Documentation is updated for changed behavior.
- No unrelated refactors or formatting churn were introduced.

## Agent-Specific Rules

Agents must:

- Read the relevant design docs before editing.
- Inspect existing code before proposing changes.
- Prefer existing patterns over new abstractions.
- Keep changes small and reviewable.
- Update tests and docs in the same change.
- Report command failures clearly.
- Preserve user changes in the worktree.

Agents must not:

- Invent undocumented product semantics.
- Use broad destructive git commands.
- Remove tests to make a build pass.
- Silence errors without surfacing user impact.
- Add dependencies without approval.
- Commit generated media fixtures unless explicitly instructed.
