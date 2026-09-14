# Pause / Fortsetzung – 14. September 2026

Auf Nutzerwunsch nach dem Tagesabschluss pausiert. Keine Hintergrundarbeit und
kein automatischer Neustart. Der gemeinsame Checkpoint umfasst die seit D06
erarbeiteten, bisher uncommitteten Änderungen; keine rückwirkend erfundenen
Einzelpaketcommits.

## Stand und Priorität

- Fundament vor weiteren CAD-Funktionen, insbesondere keine still falsch
  zugeordneten Flächen. FreeBSD/Sway bleibt Entwicklungsziel; Meridian erst L07.
- D07 **done als Fehlerfundament**: typisierte Kommandos, atomare Fehler-/
  Historygrenzen, aktuelle vs. zurückbehaltene Geometrie, explizite Fehlercommits,
  Solver-/UI-Gates. Vollständiges A-ERR-01 benötigt noch R01/U10/M15/M17/M18.
- D08 **done als isolierter Entscheidungsnachweis**: private OCAF-Autorität D12,
  native BinOcaf-Persistenz mit gemeinsamer Schreibversion 12 in D13.
  Austausch OCCT 8.0.1 ↔ 7.9.3 einschließlich Rückschreiben geprüft.
  Kein produktiver Backend-/Persistenzwechsel! Die FreeBSD-Probe verwendet das
  vollständige System-OCCT; der Produktbuild weiterhin den eigenen Lean-Prefix.
  Vor D12 dort die benötigten OCAF-Toolkits separat nachweisen, niemals mischen.
- D09 **done**: isolierter Legacy-Importer 1–16, strukturell validierter Snapshot,
  deterministischer Bericht, strikter Formatdispatch und atomarer Importbericht.
- D10 **in-progress**, Teilnachweise D10a/D10b1/D10b2a vorhanden.
  Numerischer Über-/Unterlauf abgesichert; nicht orthonormale Produkteingaben
  abgewiesen, Altachsen unverändert behalten und Geometrieauswertung gesperrt;
  explizite Ebenenkorrektur mit Undo/Redo geprüft.
- D10b2a führt Grad-/Radiant-Typen an den Rechengrenzen ein. Die Dokumentfelder
  bleiben derzeit legacy-gradwertig. Das ist noch **keine vollständige Radiant-
  Migration** und keine D10-/A-NUM-01-Abnahme.
- A02/A04 sowie spätere DAG-, Topologiereferenz-, Größen-/Toleranz- und
  Persistenzpakete bleiben entsprechend Manifest offen. Gates nicht pauschal grün.

## Morgen hier weitermachen

1. [D10b2](implementation-plan/evidence/D10b2.md) und
   [D10b](implementation-plan/evidence/D10b.md) lesen.
2. Typisierte Winkel bis in Dokument/Application führen. Die bisher in Grad
   gespeicherten RigidTransform-Rotationen **und** SketchConstraintKind::angle
   brauchen ausdrücklich konvertierende Legacy-Reader/Writer. Arc/Ellipse sind
   bereits Radiant und dürfen nicht doppelt konvertiert werden.
3. Grad-Eingaben in FeaturePropertiesPanel, Skizzendimensionen und
   TransformManipulator/MainWindow passend abgrenzen. Vorzeichen, mehrere
   Umdrehungen, UI-Snap und Undo/Redo prüfen; Altversionen behalten ihre Bedeutung.
4. Keine automatische Normalisierung alter Ebenen: Wegen der bisherigen
   Interpretation in Extrusion und Supportprojektion kann sie Geometrie ändern.
   D10b1 wählt die Ablehnung nicht kanonischer neuer Eingaben. Altwerte bleiben
   mit Diagnose erhalten; ein Migrationsassistent ist nicht implementiert.
5. Danach D10c/D10d laut [D10-Restaufträgen](implementation-plan/evidence/D10.md).
   Kein weiterer Funktionsausbau vor diesem Fundament.

## Nachweise

Aktueller Abschlussnachweis in
[D10b2](implementation-plan/evidence/D10b2.md): **Windows 49/49**, **FreeBSD/Sway 45/45** CTests bestanden. Source-Limits 226 Dateien bestanden, 94 geprüfte Quellhashes identisch, diff --check sauber.
Die zuletzt tatsächlich betrachteten CAD-Aufnahmen sind
[D10b1 Windows](implementation-plan/evidence/D10b-artifacts/windows/legacy-plane-migration.png)
und [FreeBSD/Sway](implementation-plan/evidence/D10b-artifacts/freebsd-sway/legacy-plane-migration.png).
D10b2a ändert keine Oberfläche; vollständige GUI-Regressionssuiten bleiben Pflicht.

## Umgebung und bereits erteilte Freigaben

Windows: `D:/300_Projekte/310_Aktiv/Völund`, Branch `main`.
Setup `tools/setup-windows.ps1 -SkipConfigure -PassThru` und Build/Test im
gleichen Prozesskontext wegen `V:`-Mapping. Produktbuild
`V:/build/p02-qt-final`, `tools/test-windows.ps1`; Qt-OpenGL/OCCT 8.0.1.
Keine direkte Probe ohne ihre DLL-Bereitstellung starten. D08-Probe hat dafür
einen eigenen Runtime-Deploy-Schritt; kein dev-PATH als Laufzeitnachweis.

FreeBSD: `eduard@192.168.1.238`, SSH-Schlüssel
`$env:USERPROFILE/.ssh/meridian_freebsd`, `IdentitiesOnly=yes`, `BatchMode=yes`.
Die Übertragung von Quellständen und Tests im isolierten Verzeichnis ist vom
Nutzer für die gesamte Session freigegeben; keine erneute Routinefrage.
Zielbaum `/home/eduard/src/voelund-p04-20260913`; ursprüngliches Checkout
`/home/eduard/src/voelund` nicht verändern.
Preset `freebsd-clang-lean-occt`, Qt 6.11.1 / OCCT 7.9.3.
GUI: `env -u DISPLAY XDG_RUNTIME_DIR=/var/run/xdg/eduard WAYLAND_DISPLAY=wayland-1 QT_QPA_PLATFORM=wayland`.
Quelltransfer mit SHA-256 verifizieren. Kein Python, keine neue Runtime-Abhängigkeit.

Alle Arbeiten/Prüfungen vor dem Pause-Commit beenden; keine aktiven Testprozesse
oder geplanten Fortsetzungen zurücklassen. Kein Push im Tagesabschluss.
