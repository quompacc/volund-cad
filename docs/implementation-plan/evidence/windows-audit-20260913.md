# Windows-Audit vom 13.09.2026

Codebasis: `1b78229`. Dieser Bericht belegt die Untersuchung, nicht die Erfüllung der neuen Planpakete.

## Umgebung und Grenzen

Windows, Visual Studio 18 Community, MSVC 19.51.36256, Ninja, Qt-OpenGL-Backend, Debug. Vorhandene lokale Qt-/vcpkg-Dependencies verwendet; keine vollständige Installation auf leerem Rechner. Direkter Unicode-Quellpfad konfigurierte, scheiterte aber beim Linken am verstümmelten Boost-Library-Pfad. Der frische Build über das geprüfte ASCII-Mapping war erfolgreich.

## Reproduktionsweg

In einer Visual-Studio-Developer-PowerShell arbeiten. `V:` nur verwenden, wenn frei oder nachweislich genau dieses Projekt. Projektmarker: `voelund-cad-root-6e25701d-ec07-4e9f-95c4-390df153a07c`. Ein fremdes Mapping nicht ersetzen.

```powershell
# Nur bei freiem V: ausführen; danach Marker kontrollieren.
subst V: 'D:\300_Projekte\310_Aktiv\Völund'
Get-Content V:\.voelund-project-root

cmake -S V:/ -B V:/build/audit-ascii-20260913 -G Ninja `
  -DCMAKE_BUILD_TYPE=Debug `
  -DVOELUND_VIEWPORT_BACKEND=qt-opengl `
  -DCMAKE_TOOLCHAIN_FILE=V:/.tools/vcpkg/scripts/buildsystems/vcpkg.cmake `
  -DVCPKG_TARGET_TRIPLET=x64-windows-voelund `
  -DVCPKG_INSTALLED_DIR=V:/.tools/vcpkg_installed `
  -DVCPKG_MANIFEST_MODE=OFF `
  -DCMAKE_PREFIX_PATH=V:/.tools/qt
cmake --build V:/build/audit-ascii-20260913
ctest --test-dir V:/build/audit-ascii-20260913 --output-on-failure `
  --output-junit V:/build/audit-tests.xml
```

Ergebnis: 210/210 Buildschritte, 16/16 Tests bestanden, ca.9 Sekunden CTest-Laufzeit. Lokale Rohlogs: `build/audit-configure.log`, `build/audit-build.log`, `build/audit-tests.xml`.

## Nachweis des falsch grünen Architekturchecks

```powershell
# Erwartetes tatsächliches Ergebnis im Audit: Fehler, vier Quellen >600 Zeilen.
cmake '-DVOELUND_SOURCE_ROOT=D:/300_Projekte/310_Aktiv/Völund' `
  -P tools/cmake/CheckSourceLimits.cmake

# Tatsächliches fehlerhaftes Ergebnis im Audit: Erfolg, weil keine Quellen gefunden.
cmake '-DVOELUND_SOURCE_ROOT="D:/300_Projekte/310_Aktiv/Völund"' `
  -P tools/cmake/CheckSourceLimits.cmake
```

Betroffen: DocumentSerializer.cpp601, CadViewport.cpp698, CadViewportQtOpenGl.cpp699, CadSessionTests.cpp658. A01 beseitigt die Fehlprüfung; A03 beseitigt die aufgedeckten Größenverstöße. Kein Paket in diesem Plan wurde durch das Audit bereits umgesetzt.
