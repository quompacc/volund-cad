# Differential tests

Versioned reference fixtures generated outside the product build belong here.
Every imported reference records its generator/version, source input, license and
checksum in `docs/provenance/`; the normal test run does not start FreeCAD or
Python. Differential tests use the `differential` CTest label once fixtures are
introduced.
