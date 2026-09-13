# Integration tests

Cross-module, OCAF persistence, transaction, and OCCT integration tests belong
here. Tests use the `integration` label plus their owning layer label. Temporary
files must be created through `tests/TestPaths.hpp`, never relative to the
process working directory.

`application.recompute_history` runs all 21 original CadSession cases through
`CadSessionTests.cpp`. Their implementations are grouped in
`CadSessionHistoryTests.cpp`, `CadSessionEvaluationTests.cpp`, and
`CadSessionPersistenceTests.cpp`; shared geometric helpers live in
`CadSessionTestSupport.hpp`. The exact case mapping is recorded in the A03 evidence.

`application.sketch_slot_dimensions` checks slot dimensions and actual endpoint
constraint residuals after confirmation, including rotated slots.
