# PlaneGCS in Völund CAD

This folder is the complete source of the `voelund_planegcs` library shipped in this package
(`bin/voelund_planegcs.dll` on Windows, `lib64/libvoelund_planegcs.so` on Linux). Licence:
LGPL-2.1-or-later, full text in `COPYING.LIB`.

- `upstream/`: unchanged files from FreeCAD 1.1.1, commit
  `0108fd4b4850cc46e625b60e53cea7a7bbe69f8d`, `src/Mod/Sketcher/App/planegcs/`, plus
  `SketcherGlobal.h` and `boost_graph_adjacency_list.hpp` from the same commit.
- `compat/`: small headers written for Völund that replace FreeCAD's export macros, logging
  and timing.
- `CMakeLists.txt`: the build used for this package.

## Building a replacement

You need CMake 3.28 or newer, Eigen 3.4 or newer, Boost.Graph 1.75 or newer and the same
compiler family as the package (Windows: MSVC x64; Linux: GCC with libstdc++).

1. Create a `CMakeLists.txt` next to this folder:

   ```cmake
   cmake_minimum_required(VERSION 3.28)
   project(planegcs LANGUAGES CXX)
   find_package(Eigen3 CONFIG REQUIRED)
   find_package(Boost 1.75 REQUIRED COMPONENTS graph)
   add_subdirectory(planegcs)
   ```

2. Build it in Release mode: `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release` and
   `cmake --build build`.
3. Replace the library in the package with the one you built and keep its file name.

Völund uses the solver classes `GCS::System`, `GCS::Point`, `GCS::Line`, `GCS::Circle`,
`GCS::Arc` and `GCS::Ellipse`. A replacement must keep their interface.
