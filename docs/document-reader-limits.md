# Document reader limits and migration (D02)

`DocumentSerializer::read` builds a separate candidate document and returns only
after structural validation and complete input consumption. The caller's existing
document is untouched when reading throws. Binding that result to the active UI,
path and dirty state remains D03.

## Input budget

| Bound | Current limit |
|---|---:|
| Source bytes of the old text format, including whitespace and line endings | 64 MiB |
| Bytes of a native document (I02; the imported solids up to 960 MiB) | 1 GiB |
| Solids per import (I02) | 10,000 |
| Bytes per line, excluding LF, including optional CR | 64 KiB |
| Decoded feature-name bytes | 4096 |
| Feature count | 10,000 |
| Lines/records over the entire stream, including blank lines | 200,000 |
| Feature dependency depth, including root | 256 |
| Declared sketch elements per sketch | 1,000,000 |
| Declared constraints per sketch | 2,000,000 |
| Declared extrusion profiles per extrusion | 1,000,000 |
| Auxiliary values per constraint | 4 |

All limits compose: the whole-stream record budget is tighter than the historical
per-sketch ceilings. No element/constraint/profile allocation is reserved merely
because an input count requests it. Storage grows only for successfully parsed
records; the input buffer rejects oversized lines before `getline` can grow without
bound. A source may supply one extra byte to detect a limit violation. No seeking,
filesystem size metadata or whole-file read is required.

The former feature ceiling of 100,000 is deliberately reduced to 10,000 to bound
the existing linear preceding-feature searches. Constraint reference lookup now
uses an ordered index instead of scanning all elements for every reference; profile-ID uniqueness uses an ordered set. The
format is flat; backward-only feature references forbid cycles, and the explicit
depth cap protects later recursive consumers. These are initial supported input
limits, not a benchmark or an unlimited-model claim. Raising them requires resource
measurements and updating the regression tests.

## Validation and compatibility

- Versions 1–16 retain their existing migration behavior, tested by the fixed
  [version fixtures](../tests/fixtures/documents/README.md). Unknown versions fail.
- LF and CRLF work independently of the host's text mode. A final LF is optional.
  NUL bytes, trailing non-whitespace and source I/O errors are rejected.
- IDs/counts use unsigned decimal tokens without sign or trailing characters;
  integer overflow and exhausted next-ID counters fail before document acceptance.
- Existing positive/finite geometry and finite transform/constraint validation
  remains active. Sketch directions are normalized before their dot product to
  avoid huge finite values bypassing orthogonality checks through overflow.
- Feature IDs, sketch element IDs and constraint IDs must be unique. References
  must resolve to the expected existing entities, with constraint-specific point
  and geometry rules. A constraint cannot precede its format introduction.
- Extrusion profile IDs remain opaque positive unique IDs at this layer. Whether
  an ID resolves to a computed region belongs to feature evaluation, not this parser.

## Explicit boundary for subsequent Save integration

The writer is not changed in D02. D03 must avoid committing snapshots that violate
the reader's supported bounds; a serialized candidate can be checked before the
atomic replacement. These tests do not claim an application-level Save/Open
transaction, successful recomputation of every structurally valid model, fuzzing
coverage, hardware I/O fault injection or a cryptographic file-integrity check.
