# Verbindlicher Umfang der Qualitätsprüfung

Stand: 13. September 2026, Zyklomatik und Pflichtprüfungen (A02/A04) 17. Oktober 2026. Dieser Text ist die Ausführungsentscheidung aus A02;
die maschinenlesbaren Pfade und Ausnahmen stehen in
[`tools/cmake/QualityScope.cmake`](../tools/cmake/QualityScope.cmake).

## Geltungsbereich und Ausnahmen

Die Prüfung erfasst alle selbst gepflegten C++, Header, CMake-, Shader-, UI- und
Testquellen unter `apps/`, `include/`, `src/`, `tests/`, `tools/` und
`third_party/planegcs/compat/` sowie die drei Root-Konfigurationsdateien aus
`VOELUND_QUALITY_OWNED_FILES`. Damit sind insbesondere die Völund-eigenen
PlaneGCS-Kompatibilitätsheader eingeschlossen.

Nicht Teil des Produktscopes sind nur `build/`, `.git/`, `artifacts/`, der
eingefrorene Phase-0-Spike `spikes/phase-0-freecad/` und der unveränderte
PlaneGCS-Upstream `third_party/planegcs/upstream/`. Die beiden letztgenannten
Grenzen entsprechen Architekturvertrag Abschnitt 2 und ADR 0005. Eine neue
Einzelausnahme steht ausschließlich als Eintrag `Pfad | Grund | Prüfungen` in
`VOELUND_QUALITY_EXCEPTIONS`; eine leere Liste bedeutet keine Einzel-Ausnahmen.
Breite Globs, unsichtbare Baselines und eine Ausnahme ohne Entfernungs- oder
Geltungsgrund sind unzulässig.

## Pflicht-CI (C01, ADR 0011)

`.gitea/workflows/ci.yml` läuft bei jedem Push und Pull Request auf dem
Fedora-Runner (`runs-on: linux-desktop`) und führt `tools/run-ci-linux.sh` aus:
Konfiguration `linux-desktop`, Quellgrenzen (`check-source-limits`), Build und
vollständiger CTest in einer headless Wayland-Sitzung (`kwin_wayland --virtual`).
Logs, JUnit, EGL-/Hostdaten und Smoke-Aufnahmen stehen als Artefakt
`linux-desktop-<sha>` am Lauf; die Sitzungsart steht in `session.txt`. Ein headless
Lauf ist kein Compositor- oder Displaynachweis. `main` wird nur mit grünem Lauf
geändert. Windows prüft je Paket lokal mit `tools/test-windows.ps1`; ein
Windows-Runner auf der Entwicklungsmaschine wäre ein späterer Folgeschritt.
FreeBSD läuft nicht je Push, sondern an Gate-Abnahmen.

## Regeln und Werkzeuge

| Regel | Harte Grenze | Mechanismus |
|---|---:|---|
| Physische Dateilänge | 600 | `CheckSourceLimits.cmake` |
| Funktionszeilen | 80 | `clang-tidy` `readability-function-size.LineThreshold` |
| Parameter | 5 | `clang-tidy` `readability-function-size.ParameterThreshold` |
| Verschachtelung | 4 | `clang-tidy` `readability-function-size.NestingThreshold` |
| Zyklomatische Komplexität (McCabe) | 10 | `voelund-cyclomatic` (Clang-CFG, `tools/cyclomatic`) über `CheckCyclomaticComplexity.cmake` |

Die lokale Inventur ergab LLVM `clang-tidy` 22.1.3 und `clang-format` 22.1.3
unter dem Visual-Studio-LLVM-Pfad. Beide stammen aus LLVM und stehen unter der
[Apache License 2.0 mit LLVM Exceptions](https://llvm.org/LICENSE.txt).
`readability-function-size` liefert AST-basierte Zeilen-, Parameter- und
Verschachtelungswerte und wird dafür verwendet. Seine bisher konfigurierte
Prüfung `readability-function-cognitive-complexity` ist eine **kognitive** Metrik
und wurde deaktiviert: Sie ist nicht McCabes zyklomatische Komplexität und darf
nicht unter diesem Namen als Vertragserfüllung erscheinen.

## Zyklomatische Komplexität (A02)

**Werkzeug.** `tools/cyclomatic` ist ein kleines eigenes Build-Werkzeug,
`voelund-cyclomatic`. Es gehört nicht zum CAD-Produkt und wird nicht installiert.

- Es nutzt Clangs LibTooling: Parser, AST und `clang::CFG` aus `clang-cpp`/LLVM.
- Lizenz: Apache License 2.0 mit LLVM Exceptions.
- Bezugsweg sind die Distributionspakete `clang-devel` und `llvm-devel`. Geprüft ist es auf
  Fedora 44 mit LLVM 22.1.8.
- Die Version ist auf **LLVM 22** festgelegt. Clangs C++-API ist zwischen Hauptversionen nicht
  stabil; ein Wechsel heißt neu bauen und die Fixtures erneut laufen lassen.
- Windows hat derzeit keine Clang-Entwicklungsbibliotheken (nur `clang-tidy`/`clang-format`
  aus Visual Studio). Die Prüfung läuft deshalb auf Fedora.

```
cmake -S tools/cyclomatic -B <dir> -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build <dir> && ctest --test-dir <dir>      # quality.cyclomatic_fixtures
```

**Aufruf.** Die Optionen sind die gewohnten von Clang-Tools.

- `voelund-cyclomatic [--limit=10] [--root=<dir>]... [--exclude=<dir>]... [--report-all] -p <build> <dateien>`.
- `-p` nimmt `compile_commands.json`. Mit `-- <compilerargumente>` statt `-p` geht es ohne
  Datenbank.
- Mit `--root` zählen alle Funktionen, die unter einer der Wurzeln deklariert sind, auch in
  Headern. Sie zählen je Ort einmal, egal in wie vielen Übersetzungseinheiten sie vorkommen.
  Ohne `--root` zählen nur Funktionen der übergebenen Dateien; Systemheader nie.
- Ausgabe: je Verstoß `pfad:zeile:spalte: error: cyclomatic complexity N of 'name' exceeds limit 10`
  auf stderr. `--report-all` gibt jeden Wert aus (`pfad:zeile:spalte: name: N`), dazu eine
  Summenzeile.
- Exitcodes:
  - `0`: alles innerhalb der Grenze;
  - `1`: mindestens ein Verstoß;
  - `2`: Analysefehler. Eine Übersetzungseinheit parst nicht oder für eine Funktion entsteht kein
    CFG.

  Analysefehler und Grenzverletzung sind damit getrennt.
- `CheckCyclomaticComplexity.cmake` ist der Pflichtpfad. Es scheitert ohne
  `VOELUND_CYCLOMATIC_TOOL`; einen Textersatz gibt es nicht.

**Metrik.**

- Gemessen wird McCabe als **M = E − N + 2** über den vom Eintrittsblock erreichbaren Teil von
  Clangs CFG. Das ergibt 1 plus je eine Entscheidung.
- Als Entscheidung zählt jedes `if`, jede Schleife (`for`, Bereichs-`for`, `while`, `do`), jedes
  `case`, jedes `catch`, `&&`, `||` und `?:`.
- `switch` ohne `default` zählt wie mit `default`: jeder `case` eins.
- Bedingungen mit konstantem Wert zählen trotzdem (`PruneTriviallyFalseEdges` aus).
- Implizite Destruktor- und Ausnahmekanten des CFG zählen nicht; sie sind kein geschriebener
  Kontrollfluss.
- Ein `try` gilt einmal als betreten, jeder Handler ist ein Zweig. Der Weg „kein Handler passt“
  zählt nicht.
- Unerreichbarer Code, zum Beispiel nach `return`, zählt nicht.
- `for (;;)` ohne Bedingung ist keine Entscheidung; nur ihr Ausstieg über ein `if` zählt.
- **Lambdas** sind eigene Funktionen (`umgebend::(lambda)`). Die umgebende Funktion zählt sie nicht.
- **Templates** werden einmal am ungebundenen Muster gemessen; Instanziierungen nicht noch einmal.
  Für manche Muster baut Clang kein CFG, etwa bei einer Bereichs-`for`-Schleife über einen
  abhängigen Typ. Dann zählt die höchste Instanziierung der Übersetzungseinheit. Ohne
  Instanziierung ist das ein Analysefehler.
- `=default`- und implizite Funktionen zählen nicht.
- **Makros** zählen expandiert.
- **Präprozessor:** gemessen wird unter der Konfiguration der jeweiligen
  Compile-Datenbank, also genau die Zweige, die dieser Build übersetzt.

**Nachweis.** `tests/cmake/CyclomaticComplexityTests.cmake` legt die Sollwerte vorab fest.

- Geprüft werden 18 Kontrollflussfälle mit exakten Werten: gerade Funktion 1, `if` 2,
  `(a && b) || c` 4, `?:` 2, `switch` mit/ohne `default` 4/3, vier Schleifenarten 5, Lambda 2
  (umgebend 1), Template 2 (ohne Instanzdoppel), Template mit abhängiger Bereichs-`for`-Schleife 2, `try` mit zwei `catch` 3, Code nach `return` 1,
  `if (true)` 2, Makro mit `?:` 2, Endlosschleife mit `if` 2.
- Über das Pflichtskript: genau 10 besteht, 11 scheitert mit Wert und Name. Ein Parse-Fehler
  scheitert als Analysefehler, nicht als Verstoß. Ein fehlendes Werkzeug scheitert eigens.
- `QualityRulesTests.cmake` prüft weiterhin den Fehlerpfad ohne Werkzeug neben den
  `clang-tidy`-Regeln.
- Die Anbindung an das Gesamtziel und an die CI gehört zu A04.

`tests/cmake/QualityRulesTests.cmake` enthält je eine saubere Fixture sowie
absichtliche Verstöße für Dateilänge, Funktionslänge, Parameter,
Verschachtelung sowie den Fehlerpfad der Zyklomatik ohne Werkzeug.

## Pflichtprüfungen (A04)

| Prüfung | Lokal | CTest | CI |
|---|---|---|---|
| Format | `cmake --build <build> --target format-check` | `architecture.format` | über CTest |
| Schichten (Includes) | `cmake --build <build> --target layer-check` | `architecture.layer_includes` | über CTest |
| Schichten (Links) | beim Konfigurieren (`voelund_check_layer_links`) | – | Konfiguration |
| Funktionsgrenzen | `sh tools/run-static-analysis.sh <build> [<ausgabe>]` (Linux) | – | eigener Schritt nach CTest |
| Fixtures | – | `architecture.layer_and_format_regression`, `architecture.function_limits_regression` | über CTest |

**Format.**

- `CheckFormat.cmake` prüft alle C++-Dateien der eigenen Wurzeln mit `clang-format --dry-run --Werror`.
- Verlangt ist `clang-format` 22, unter Windows aus Visual Studio, unter Fedora aus dem
  Distributionspaket. Ein fehlendes Werkzeug lässt den Test scheitern, statt ihn auszulassen.
- Der Altbestand wurde einmal in einem reinen Format-Commit bereinigt. Neue Abweichungen schlagen
  sofort fehl.

**Schichten (K01).**

- Die Regeln stehen in `tools/cmake/VoelundLayers.cmake`: welche Schicht welche einbinden und
  linken darf.
- Unterhalb der Oberfläche gibt es kein Qt, also in `kernel`, `platform`, `document`, `features`,
  `application`, `workspaces` und `modules`.
- Relative Includes aus dem eigenen Verzeichnis heraus sind verboten.
- `CheckLayerIncludes.cmake` liest die `#include`-Zeilen von `src/<schicht>` und
  `include/voelund/<schicht>`.
- `voelund_check_layer_links()` prüft beim Konfigurieren die Link-Kanten der Schichtziele.

**Funktionsgrenzen.**

- `tools/run-static-analysis.sh` baut `tools/cyclomatic` mit dem Plugin `VoelundTidyPlugin`.
- Danach läuft `clang-tidy` einmal über alle eigenen Übersetzungseinheiten der Compile-Datenbank,
  mit `readability-function-size` und `voelund-cyclomatic-complexity`. Das Plugin nutzt
  denselben Messcode wie `voelund-cyclomatic`; die Fixtures prüfen den Gleichstand.
- Header zählen mit: `HeaderFilterRegex` passt auf absolute Pfade der eigenen Wurzeln.
- Fedora braucht dafür `clang-tools-extra-devel` (Header für `clang-tidy`-Plugins, Build-only).
- `CheckFunctionLimits.cmake` vergleicht das Ergebnis mit der sichtbaren Ausnahmeliste
  [`tools/quality/function-limits.txt`](../tools/quality/function-limits.txt).
- Die Liste hat das Format `regel | pfad | funktion | wert | grund`.
- Regeln und Werte:
  - `lines`: Funktionszeilen;
  - `parameters`: Anzahl der Parameter;
  - `nesting`: Stellen tiefer als 4;
  - `cyclomatic`: McCabe-Wert.

**Was scheitert** (Nutzerentscheid 17.10.2026):

- eine neue Funktion über einer Grenze;
- ein gelisteter Wert, der steigt;
- ein gelisteter Eintrag, der nicht mehr gemessen wird, weil er behoben, umbenannt oder gelöscht
  ist. Die Liste schrumpft so nur mit dem Code.
- ein Analysefehler;
- ein fehlendes Werkzeug.

**Abbau.** Die Liste wird schrittweise abgebaut, zuerst die höchsten Werte im Produktcode.
`<ausgabe>/function-limits-current.txt` enthält nach jedem Lauf den gemessenen Stand im
Listenformat. Daraus lässt sich die Liste nach einer Bereinigung übernehmen.
