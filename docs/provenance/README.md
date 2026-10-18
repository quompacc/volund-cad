# Source provenance

Record source URL, repository commit, file, license, and the nature of every directly reused or materially derived implementation.

## Records

- [FreeCAD PlaneGCS](freecad-planegcs.md)
- [occt-samples-qt (Qt OpenGL context)](occt-samples-qt.md)
- [Embedded fonts](fonts.md)
- [NIST PMI STEP test files](nist-pmi-step.md) (tests only, not shipped)

## Shipped package (L04)

`packaging/Components.cmake` assigns every file of the package to a component with version,
licence and the notices it needs. The licence texts are in `packaging/licenses/` (their
origin in its README), and the package carries `THIRD-PARTY-NOTICES.md` and a `MANIFEST.txt`
written from the staged files. `platform.package_runtime` fails when a file belongs to no
component, a notice is missing, or the manifest and the package differ.

These checks show that the planned accompanying material is present. They are not a legal
clearance.
