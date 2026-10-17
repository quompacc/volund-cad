# Differential tests

Versioned reference fixtures generated outside the product build belong here.
Every imported reference records its generator/version, source input, license and
checksum in `docs/provenance/`; the normal test run does not start FreeCAD or
Python. Differential tests use the `differential` CTest label.

| Test | Reference | What it shows |
|---|---|---|
| `differential.nist_step` | `nist/` — seven NIST MBE PMI parts (AP242) with the geometric validation properties their exporting CAD systems wrote (`nist/reference.txt`) | STEP import reproduces volume and area within 1e-5 and the centroid within 0.01 mm; export and re-import twice stays within 1e-5 |
| `differential.nist_step_detects` | `nist/reference-wrong.txt` (CTC-03 volume 1e-4 off) | the comparison fails (`WILL_FAIL`) |
| `differential.freecad_parts` | `freecad/reference.txt` from `tools/reference-freecad/make_references.py`, known deviations in `freecad/known-differences.txt` | plate with rounded corners and through hole, blind hole, chamfered top edges, mirrored join, joined linear pattern, shell and full revolve rebuilt with Völund features: same volume, area, centroid and face count |

Provenance: [docs/provenance/nist-pmi-step.md](../../docs/provenance/nist-pmi-step.md).
