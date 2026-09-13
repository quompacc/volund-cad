# Regression tests

Every fixed defect receives a minimal reproducible fixture in this directory.
Each fixture has a neighbouring Markdown record named `<defect-id>.md` with:

- stable defect ID and date;
- minimized input/reproduction command;
- expected observable invariant and former failure;
- owning test name and labels;
- affected platform/configuration and any remaining boundary.

The product build neither runs FreeCAD nor Python to create these fixtures.

Current records:

- [F01 – source-limit checker](F01-source-limit.md)
- [F02 – save safety, prepared for D01–D03](F02-save-safety.md)
