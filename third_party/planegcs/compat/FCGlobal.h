#pragma once

// L04 (user decision 2026-10-18): PlaneGCS ships as its own shared library, so that it can be
// replaced (LGPL-2.1). Its public classes carry SketcherExport, which maps to these macros.
#if defined(_WIN32)
#define FREECAD_DECL_EXPORT __declspec(dllexport)
#define FREECAD_DECL_IMPORT __declspec(dllimport)
#else
#define FREECAD_DECL_EXPORT __attribute__((visibility("default")))
#define FREECAD_DECL_IMPORT __attribute__((visibility("default")))
#endif
