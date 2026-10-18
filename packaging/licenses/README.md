# Licence texts shipped with the package (L04)

Unchanged copies of the licence texts of the components that the package contains or that are
compiled into it. `packaging/CMakeLists.txt` installs them under `licenses/`, and
`packaging/Components.cmake` names which file each component needs. A missing file makes the
package check (`platform.package_runtime`) fail.

| Folder | Component | Copied from | SHA-256 |
|---|---|---|---|
| `opencascade/LICENSE_LGPL_21.txt` | Open CASCADE Technology | lean OCCT 7.9.3 build, `share/doc/opencascade/` (identical text in the vcpkg 8.0.1 port's `copyright`) | `e237fa56668030e928551ddd60f05df5fe957f75eab874bbd017e085ed722e7c` |
| `opencascade/OCCT_LGPL_EXCEPTION.txt` | Open CASCADE Technology | as above | `04580a884ea6cea294402649ff7b5cbb167d47462d1340a4ed33e550db10a81b` |
| `qt/LGPL-3.0-only.txt` | Qt (Windows package) | Fedora 44 `qt6-qtbase-6.11.2`, `/usr/share/licenses/qt6-qtbase/` | `da7eabb7bafdf7d3ae5e9f223aa5bdc1eece45ac569dc21b3b037520b4464768` |
| `qt/GPL-3.0-only.txt` | Qt (LGPL-3.0 builds on it) | as above | `8ceb4b9ee5adedde47b31e975c1d90c73ad27b6b165a1dcd80c7c545eb65b903` |
| `eigen/COPYING.*` (five files) | Eigen 5.0.1 | Fedora 44 `eigen3-devel-5.0.1`, `/usr/share/licenses/eigen3-devel/` | `COPYING.MPL2`: `66a3107d5ad6a058aab753eaac2047ccb2ed0e39465dd0fe5844da3e300d5172` |
| `boost/LICENSE_1_0.txt` | Boost.Graph | Fedora 44 `boost-graph`, `/usr/share/licenses/boost-graph/` | `c9bff75738922193e67fa726fa225535870d2aa1059f91452c411736284ad566` |
| `freetype/copyright.txt` | FreeType 2.14.3 (Windows) | vcpkg port `freetype`, `share/freetype/copyright` | `cf82ed29473c1fd8e137fd34250f89eebf2bee9e126fa004286dd25495d5c654` |
| `zlib/copyright.txt` | zlib 1.3.2 (Windows) | vcpkg port `zlib` | `e32ff4e00d9d94930537635291da39e7e612703334bf6fde8c7f1686fe8a45a2` |
| `libpng/copyright.txt` | libpng 1.6.58 (Windows) | vcpkg port `libpng` | `bdb0a645ea18c60507d0368379b1ac5474b92255fcc2d115e07486a7672ba526` |
| `bzip2/copyright.txt` | bzip2 1.0.8 (Windows) | vcpkg port `bzip2` | `c6dbbf828498be844a89eaa3b84adbab3199e342eb5cb2ed2f0d4ba7ec0f38a3` |
| `brotli/copyright.txt` | Brotli 1.2.0 (Windows) | vcpkg port `brotli` | `3d180008e36922a4e8daec11c34c7af264fed5962d07924aea928c38e8663c94` |
| `occt-samples-qt/LICENSE.txt` | occt-samples-qt (adapted in `src/viewport/QtOcctGlContext.cpp`) | MIT licence text with the copyright line from the adapted file | `b4dfe9f2127e9ec51b49465b42f07a589be3da472ae818b0531d4e275b90bbca` |

Not stored here because they already sit beside their source: PlaneGCS
(`third_party/planegcs/COPYING.LIB`) and the fonts (`resources/fonts/*-OFL.txt`).

When a component's version changes, compare its licence text with the new release and update
this folder and table.
