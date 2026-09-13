# Verbindlicher Umfang der Qualitätsprüfung

Stand: 13. September 2026. Dieser Text ist die Ausführungsentscheidung aus A02;
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

## Regeln und Werkzeuge

| Regel | Harte Grenze | Mechanismus |
|---|---:|---|
| Physische Dateilänge | 600 | `CheckSourceLimits.cmake` |
| Funktionszeilen | 80 | `clang-tidy` `readability-function-size.LineThreshold` |
| Parameter | 5 | `clang-tidy` `readability-function-size.ParameterThreshold` |
| Verschachtelung | 4 | `clang-tidy` `readability-function-size.NestingThreshold` |
| Zyklomatische Komplexität (McCabe) | 10 | Clang-AST/CFG-Consumer, noch verpflichtend nicht verfügbar |

Die lokale Inventur ergab LLVM `clang-tidy` 22.1.3 und `clang-format` 22.1.3
unter dem Visual-Studio-LLVM-Pfad. Beide stammen aus LLVM und stehen unter der
[Apache License 2.0 mit LLVM Exceptions](https://llvm.org/LICENSE.txt).
`readability-function-size` liefert AST-basierte Zeilen-, Parameter- und
Verschachtelungswerte und wird dafür verwendet. Seine bisher konfigurierte
Prüfung `readability-function-cognitive-complexity` ist eine **kognitive** Metrik
und wurde deaktiviert: Sie ist nicht McCabes zyklomatische Komplexität und darf
nicht unter diesem Namen als Vertragserfüllung erscheinen.

Weder ein `clang++`-Frontend noch `clang-check` oder `clang-query` war in der
geprüften Installation vorhanden. Textsuche in CMake wäre wegen C++-Makros,
Kommentaren, Strings, Templates und unevaluierter Ausdrücke keine echte
zyklomatische Messung und wurde bewusst nicht eingeführt. Der verpflichtende
Prüfpfad `CheckCyclomaticComplexity.cmake` schlägt deshalb mit einer klaren
Meldung fehl, bis ein zugelassener LLVM-22+-Clang-AST/CFG-Consumer konfiguriert
ist. A04 verdrahtet diesen Consumer erst dann in das gesamte Pflichtziel; es
darf keine ersatzweise grüne Qualitätsprüfung geben.

`tests/cmake/QualityRulesTests.cmake` enthält je eine saubere Fixture sowie
absichtliche Verstöße für Dateilänge, Funktionslänge, Parameter,
Verschachtelung und Zyklomatik. Ohne CFG-Consumer bestätigt die letzte Fixture
den korrekten Pflichtfehler statt eine nicht vorhandene Messung zu simulieren.
