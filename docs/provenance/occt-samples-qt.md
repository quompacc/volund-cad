# Provenance: OCCT Qt OpenGL context integration

## Source

- Repository: <https://github.com/gkv311/occt-samples-qt>
- Commit: `72e32d3a8398229702030ffc22b9f847ba7ad5d0`
- Relevant files: `occt-qt-tools/OcctGlTools.h`, `occt-qt-tools/OcctGlTools.cpp`,
  `occt-qopenglwidget/OcctQOpenGLWidgetViewer.cpp`
- Original author: Kirill Gavrilov
- License: MIT, Copyright (c) 2023 Kirill Gavrilov

## Scope of adaptation

`src/viewport/QtOcctGlContext.cpp` derives the minimum required mechanics for wrapping
Qt's current OpenGL context and framebuffer in OCCT. Names, error handling, ownership,
and the surrounding viewport lifecycle are adapted to Völund's architecture. Qt input
translation, sample UI, view cube, diagnostics, and unrelated utilities are not copied.

The source-level copyright and license reference is retained in the adapted file. The
upstream repository is a design reference only and is not a build or runtime dependency.
