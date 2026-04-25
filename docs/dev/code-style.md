# Code Style

## Language and Standards

- C11.
- English for all source code identifiers, comments, and documentation.

## Conventions

- Small module APIs with explicit structs.
- Clear `bool` return values for recoverable failure.
- `NULL` checks on public or cross-module function inputs.
- No hidden global editing state.
- No blocking I/O inside decoder loops.

## Memory and Resource Standards

- Every allocation must have a visible cleanup path.
- Every FFmpeg context/frame/packet/scaler/resampler must be freed in the owning module.
- Every decoder created by project code must be destroyed by project code.
- Export threads must be joined before exporter destruction.
- Do not use project pointers after project destruction.
- Do not leak waveform peak buffers on media removal or project destruction.

## Review Checklist

- What owns this pointer?
- What happens if allocation step N fails?
- Does cleanup handle partially initialized objects?
- Can this code run while export is reading the project?
- Does this code allocate during per-frame rendering?

## Performance Rules

Per-frame UI:

- No filesystem scanning.
- No waveform generation.
- No expensive media probing.
- No unbounded allocation.

Playback:

- Seek only on scrub, hard drift, or session changes.
- Pause inactive decoders.
- Prefer audio-only/video-only decoder modes for clip decoders.

Export:

- Reuse export decoder cache.
- Update progress at deterministic intervals.
- Check cancellation regularly.

## Avoid

- UI code that directly performs long-running IO.
- Export code that depends on live preview state.
- Project save files that include runtime pointers.
- Broad refactors mixed with feature changes.
