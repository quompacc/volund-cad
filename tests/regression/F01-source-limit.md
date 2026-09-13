# F01 – Source-limit checker accepted an empty input

- **Reproduction:** invoke `architecture.source_limits_regression`, which passes
  valid, quoted, empty, missing, file and 601-line fixture roots to
  `CheckSourceLimits.cmake`.
- **Former failure:** a literal-quoted root globbed zero files and returned
  success.
- **Invariant:** an invalid root or zero discovered source files fails; a valid
  root reports its file count; a 601-line source fails.
- **Owning test:** `architecture.source_limits_regression` (`architecture`,
  `regression`).
- **Boundary:** the four pre-existing source-limit violations are A03 work and
  remain visible in the repository-wide check.
