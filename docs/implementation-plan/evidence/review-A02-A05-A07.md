# Gegenprüfung von A02, A05 und A07

13.09.2026, Arbeitsbaum auf Basis `1b78229`. Geprüft wurden die gemeldeten Änderungen und Evidenzen. Bestehende Benutzer-/Agentänderungen erhalten; keine pauschale Abnahme sämtlicher Änderungen.

**Nachtrag:** A07/R4 ist inzwischen nach Korrektur, erweiterten Regressionen,
frischem Windows-Build und CTest abgenommen; siehe [aktueller A07-Nachweis](A07.md).
Die nachfolgende Gegenprüfung dokumentiert den damaligen Befund. A02 bleibt offen.

## Ergebnis

- **A05:** Berichtete 6 Unit- und 7 Integrationstests im vorhandenen Build `build/a05-msvc-final` erneut ausgeführt:13/13 bestanden, ca.5,44s. Kein neuer vollständiger Build in dieser Gegenprüfung. Gemeinsame feste Tempdateinamen sind eine verbleibende Einschränkung für parallele Läufe verschiedener Builds; vor solcher Parallelisierung getrennte Laufverzeichnisse verwenden.
- **A02:** Teilweise umgesetzt, jetzt `needs-design`. Fehlender Clang-Frontend ist eine Umgebungslücke; der fehlende CFG-Consumer ist zusätzlich noch Implementierungsarbeit. Ein nachgewiesener, vom Agenten nicht selbst lösbarer externer Zugangs-/Berechtigungsblocker ist bisher nicht dokumentiert.
- **A07:** Zurück in `review`: Windows-Erfolgsweg belegt, aber Mapping-Gegenprobe und einzelne Dokumentations-/Voraussetzungsaufgaben sind noch offen. Dafür ist keine Nutzerprüfung erforderlich.
- **A03:** Kann nach A01 selbstständig beginnen. Die Aufteilung der vier bekannten Dateien benötigt keinen fertigen CFG-Consumer. A02/A04 und das Gesamtgate bleiben trotzdem Pflicht.

## R1 – Fehlender CFG-Consumer ist noch Entwicklungsarbeit

`tools/cmake/CheckCyclomaticComplexity.cmake` validiert eine Frontendversion und endet danach unbedingt in `FATAL_ERROR`. Eine korrekte Clang-Installation allein macht diesen Pfad daher nicht funktionsfähig. Die Zyklomatikfixture in `QualityRulesTests.cmake` testet ausschließlich „Werkzeug fehlt“, keinen Messwert und keine10/11-Grenze.

Das ist ein ehrlich fehlschlagender Zwischenzustand, erfüllt aber A02 nicht. Für die Grenze „LLVM22 oder neuer“ fehlt bisher eine technische Begründung; vorhandene LLVM22.1.3-Tools allein begründen keine Mindestversion für einen noch nicht implementierten Consumer.

Die Fortsetzung besteht aus drei begrenzten Unteraufträgen innerhalb A02:

1. **A02.1 – Werkzeugvertrag/Bereitstellung:** Verfügbare vollständige LLVM-Toolchains und erlaubte Bereitstellungswege prüfen. Versionsgebundenen Build-only-Weg mit Herkunft/Lizenz und tatsächlich benötigten Bestandteilen festlegen. Clang-Buildwerkzeuge sind keine neue CAD-Runtime. Erst bei konkretem nicht lösbarem Installations-/Zugangsfehler `blocked-external` setzen, mit Versuch und Ursache.
2. **A02.2 – Messung:** Gewählten nativen AST/CFG-Weg implementieren. CLI, Compilation-Database-Eingabe, eigene Funktionen, Ergebnisformat und Fehlercodes konkretisieren. Metrik einschließlich Short-Circuit, Switch, Lambda, Templates, impliziten/unerreichbaren CFG-Kanten und Konfigurationsabhängigkeit dokumentieren. Keine Regex-Zählung von C++-Text und keine Umbenennung kognitiver Komplexität.
3. **A02.3 – Nachweis/Scope:** Saubere Funktion plus exakt10 und11 sowie repräsentative Kontrollflussfälle mit vorher festgelegten Sollwerten prüfen. Fehlendes Werkzeug bleibt separater negativer Test. Scope vervollständigen und passende Fixtures ergänzen; Analysefehler von Grenzwertverletzung unterscheiden. A04 übernimmt die vollständige Repository-/CI-Anbindung.

Die drei Unteraufträge sind getrennt zu bearbeiten. Architektur-/Dependencyvertrag bleibt gültig; normale Detailentscheidungen innerhalb dieses Vertrags übernimmt der Agent selbst.

## R2 – Ungeprüfte rekursive Testlöschung: behoben

`QualityRulesTests.cmake` enthielt `file(REMOVE_RECURSE "${VOELUND_TEST_ROOT}")` ohne Eingrenzung des übergebenen Pfads. Ein falsch gesetzter Testpfad konnte fremde Daten löschen.

Korrektur in dieser Gegenprüfung: Jeder Lauf erzeugt ein neues eigenes Kindverzeichnis; das Elternverzeichnis wird weder gelöscht noch für feste Fixturedateinamen übernommen. Bei Namenskollision Abbruch. Keine rekursive Löschung mehr.

Neue `tests/cmake/QualityFixtureIsolationTests.cmake` führt die Qualitätsfixtures zweimal aus und prüft einen aufrufereigenen Sentinel sowie die unveränderte Fixture des ersten Laufs. Beide Läufe bleiben getrennt erhalten. Bei A04 in die verbindliche Testanbindung aufnehmen; derzeit eigenständiger nativer CMake-Test.

Ausgeführter Befehl:

```powershell
cmake '-DVOELUND_SOURCE_ROOT=D:/300_Projekte/310_Aktiv/Völund' `
  '-DVOELUND_TEST_ROOT=D:/300_Projekte/310_Aktiv/Völund/build/review-quality-fixtures' `
  '-DVOELUND_CLANG_TIDY=C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/Llvm/x64/bin/clang-tidy.exe' `
  -P tests/cmake/QualityFixtureIsolationTests.cmake
```

Tatsächlich CMake aus Visual Studios gebündeltem CMake-Verzeichnis verwendet. Exit0; aufrufereigene Daten und erster Lauf erhalten. Zusätzlich ursprünglichen Qualitätsfixturelauf mit externem Sentinel ausgeführt und dessen SHA256 vor/nachher verglichen: identisch. Diese Erfolge beweisen keine vorhandene Zyklomatikmessung. `git diff --check` bestanden.

## R3 – Scope noch unvollständig und nicht angebunden

`QualityScope.cmake` enthält noch nicht `cmake/`, Root-Presets/Dependencykonfiguration oder vollständig die eigenen Buildadapter. `CheckSourceLimits.cmake` liest diesen Scope noch nicht; `.ps1`, `.sh` und `CMakeLists.txt` fehlen in seinem bisherigen Extensionfilter. Behaupteter Scope und tatsächliche Prüfung unterscheiden sich daher.

A02.3/A04: Ein gemeinsames Inventar eigener Dateien, explizite Klassen und Ausnahmen; Fixture pro eingeschlossenem Wurzel-/Dateityp, insbesondere `cmake/vcpkg-triplets`, Root-CMake, PowerShell/Shell und eigene PlaneGCS-Adapter. C++-Komplexität nur auf C++ anwenden; physische Größen-/Buildregeln nach Dateiklasse. Kein „vollständig erfasst“ vor dem Nachweis.

## R4 – Mapping erkennt Projektmarker, aber nicht exakten Checkout

`setup-windows.ps1` akzeptiert ein vorhandenes V:-Mapping, sobald der bekannte Marker übereinstimmt. Derselbe versionierte Marker ist auch in einem anderen Checkout vorhanden. Das Skript kann dann das falsche Arbeitsverzeichnis konfigurieren. Die Kontrolle des tatsächlichen Mapping-Zielpfads fehlt; ein physisch belegtes V: darf ebenfalls nicht als erfolgreich neu zugeordnet behandelt werden.

A07: Kanonisches Mappingziel mit aktuellem Repositorypfad vergleichen; Subst-Exitcode prüfen; belegtes Nicht-Subst-Laufwerk ablehnen. Regressionen mit zwei verschiedenen Fixtureverzeichnissen und identischem Marker, korrektem Ziel, fremdem Marker und fehlgeschlagenem Mapping. Native Befehle mocken oder isolierten Adapter verwenden; keine echten fremden Laufwerkszuordnungen verändern.

In `docs/dependencies.md` steht weiterhin „Linux uses distribution packages“, direkt vor dem widersprechenden neuen vcpkg-Abschnitt. Bereinigen. Die in A07 verlangten konkreten FreeBSD-Eigen-/Boost-Voraussetzungen fehlen ebenfalls noch; Paketnamen anhand Zielumgebung/primärer Dokumentation prüfen. Nicht ausgeführte Linux-/FreeBSD-Runs bleiben offen, wie bereits korrekt berichtet.

## Fortsetzung

Manifest: A02 `needs-design`, A03 `ready` nach A01, A05 weiterhin `done`, A07 `review`. A04 wartet weiterhin auf A02 und A03. Die A03-Aufteilung erhält Semantik und Tests; die Abhängigkeitskorrektur hebt weder harte Quellregeln noch Meridian-Gates auf.

Kein Nutzer-Resttest nötig. Optische Prüfung für die vorgenommenen reinen Test-/Planänderungen nicht erforderlich. R1/R3/R4 sind konkrete Folgearbeit, nicht bereits implementierte Korrekturen.
