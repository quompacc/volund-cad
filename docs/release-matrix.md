# Release-Buildmatrix (L01)

Stand: 17. Oktober 2026. Eine Freigabe verlangt **jede** Zeile dieser Matrix grün. Solange eine
Pflichtplattform offen ist, gibt es kein grünes Gesamtsignal, auch wenn das CI grün ist.

| Plattform | Debug | Release | Ausführung | Stand |
|---|---|---|---|---|
| Linux (Fedora, GCC, OCCT 7.9.3, Wayland) | `linux-desktop` | `linux-desktop-release` | Pflicht-CI bei jedem Push (`ci / linux-desktop`) | **läuft** |
| Windows 11 (MSVC, OCCT 8.0.1, Qt OpenGL) | `windows-msvc` | `windows-msvc-release` | lokal mit `tools/test-windows.ps1 -BuildDirectory build/<preset>` (ohne Desktop-Tests) | **offen**: kein CI-Runner; Release nur als Preset vorhanden |
| FreeBSD (Clang, lean OCCT, Sway) | `freebsd-clang-lean-occt` | `freebsd-clang-lean-occt-release` | `sh tools/run-freebsd-integration.sh control <preset>` an Gate-Abnahmen | **offen**: kein CI-Runner |
| GPU-/Display-Nachweis, Meridian | – | – | eigene Abnahme (L07) | **offen**: im CI nur headless `kwin_wayland --virtual`, kein Display-Nachweis |

## Was der Pflichtlauf prüft

Das CI läuft auf dem Fedora-Runner (`.gitea/workflows/ci.yml` → `tools/run-ci-linux.sh`) und hat
diese Schritte:

1. Konfiguration `linux-desktop` (Debug) und Quellgrenzen.
2. Build und vollständiger CTest einschließlich Desktop-Tests unter headless Wayland. Dazu
   gehören Format-, Schichten- und Fixture-Prüfungen sowie die Fremdreferenzen aus Q01.
3. Statische Analyse: Funktionsgrenzen gegen `tools/quality/function-limits.txt`.
4. Konfiguration, Build und vollständiger CTest in `linux-desktop-release`.

Jeder fehlschlagende Schritt macht den Lauf rot, und ohne grünen Lauf wird `main` nicht bewegt.

## Archiv je Commit

Das Artefakt `linux-desktop-<sha>` enthält `artifacts/linux-desktop/` und
`artifacts/linux-desktop-release/` mit diesen Dateien:

- `host.txt`, das Umgebungsmanifest: Zeit, Preset, Commit, Build-Typ, OCCT-Pfad,
  Wayland-Sitzung, Kernel, Distribution, GCC, CMake, Qt und Paketversionen;
- `egl.txt`, `linked-libraries.txt` und `x11-linkage.txt`;
- `ctest.txt` und `ctest.junit.xml`;
- `smoke/`, die automatisch erzeugten Aufnahmen. Sie sind **nicht** angesehen;
- `session.txt` (nur Debug-Ordner): die Sitzungsart;
- `static-analysis/` (nur Debug-Ordner): Analysebericht und gemessener Listenstand.

## Offene Voraussetzungen

- **Windows- und FreeBSD-Runner:** Der Nutzer entscheidet später, im Zusammenhang mit dem noch
  offenen Umzug des CI-Runners. Bis dahin sind beide Plattformen hier offen. Prüfungen gibt es
  nur lokal oder an Gate-Abnahmen.
- **Windows-Release-Build:** Er ist als Preset definiert, aber noch nicht gebaut. Auf der
  Entwicklungsmaschine wird ohne Freigabe kein weiterer Build-Ordner angelegt.
