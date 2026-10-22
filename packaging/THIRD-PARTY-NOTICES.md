# Völund CAD – Bestandteile Dritter / Third-party components

Dieses Paket enthält Software Dritter. Die vollständigen Lizenztexte liegen im Ordner
`licenses/`. `MANIFEST.txt` nennt jede Datei des Pakets mit SHA-256, Komponente, Version und
Lizenz.

This package contains third-party software. The complete licence texts are in `licenses/`.
`MANIFEST.txt` lists every file of the package with its SHA-256, component, version and licence.

| Komponente | Lizenz | Herkunft | Im Paket |
|---|---|---|---|
| PlaneGCS (aus FreeCAD 1.1.1, Commit `0108fd4b4850cc46e625b60e53cea7a7bbe69f8d`) | LGPL-2.1-or-later | <https://github.com/FreeCAD/FreeCAD>, `src/Mod/Sketcher/App/planegcs` | eigene Bibliothek `voelund_planegcs` (DLL/.so), Quelle unter `source/planegcs/` |
| Open CASCADE Technology | LGPL-2.1 mit OCCT-Ausnahme | <https://github.com/Open-Cascade-SAS/OCCT> | Bibliotheken `TK*` |
| Qt (Windows) | LGPL-3.0-only | <https://download.qt.io/official_releases/qt/> | `Qt6*.dll`, `platforms/` |
| FreeType (Windows) | FreeType License (FTL) | <https://freetype.org> | `freetype.dll` |
| zlib, libpng, bzip2, Brotli (Windows, von FreeType benötigt) | Zlib, libpng-2.0, bzip2-1.0.6, MIT | vcpkg-Ports | jeweilige DLL |
| Eigen | MPL-2.0 (Teile Apache-2.0, BSD-3-Clause, MINPACK) | <https://gitlab.com/libeigen/eigen> | in PlaneGCS einkompiliert (nur Header) |
| Boost.Graph | BSL-1.0 | <https://www.boost.org> | in PlaneGCS einkompiliert (nur Header) |
| Albert Sans, JetBrains Mono | SIL OFL 1.1 | <https://github.com/google/fonts> | in Völund eingebettet, unverändert |
| occt-samples-qt (Kirill Gavrilov) | MIT | <https://github.com/gkv311/occt-samples-qt>, Commit `72e32d3a` | Teile in Völunds Viewport angepasst |
| Microsoft Visual C++ Laufzeit (Windows) | Visual Studio Redistributable | Microsoft | `msvcp140*.dll`, `vcruntime140*.dll`, `concrt140.dll` |

Die genauen Versionen dieses Pakets stehen in `MANIFEST.txt` (Zeilen `component`).

## Austausch der LGPL-Bibliotheken / Replacing the LGPL libraries

Qt, Open CASCADE und PlaneGCS sind als eigene, dynamisch geladene Bibliotheken enthalten. Sie
lassen sich durch eigene, ABI-kompatible Builds ersetzen. Für PlaneGCS liegt die vollständige
Quelle samt Build-Anleitung unter `source/planegcs/`.

Qt, Open CASCADE and PlaneGCS are shipped as separate shared libraries and may be replaced by
compatible builds of your own. The complete PlaneGCS source with build notes is in
`source/planegcs/`.

## Quellcode / Source code

- **PlaneGCS:** liegt diesem Paket bei (`source/planegcs/`).
- **Qt und Open CASCADE:** werden unverändert in der in `MANIFEST.txt` genannten Version
  ausgeliefert. Ihre Quellen sind bei den oben genannten Herkünften erhältlich.

Auf Anfrage stellen wir für mindestens drei Jahre nach Auslieferung dieses Pakets den
vollständigen Quellcode der hier enthaltenen LGPL-Bibliotheken in der ausgelieferten Version
gegen Erstattung der Kopierkosten bereit. Anfrage an die Herausgeber von Völund CAD.

On request, for at least three years after this package was distributed, we provide the
complete source code of the LGPL libraries contained here, in the version shipped, for no more
than the cost of copying. Ask the publishers of Völund CAD.

## Völund CAD

Völund CAD selbst steht unter der GNU General Public License, Version 3 oder (nach Ihrer Wahl)
jeder späteren Version (GPL-3.0-or-later), ohne jede Gewährleistung. Den Lizenztext enthält
`licenses/voelund/LICENSE`.

Völund CAD itself is licensed under the GNU General Public License, version 3 or (at your option)
any later version (GPL-3.0-or-later), without any warranty. The licence text is in
`licenses/voelund/LICENSE`.
