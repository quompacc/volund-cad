# Fortsetzung – 16. September 2026

## Verbindlicher Stand

D10 abgeschlossen. **D11a–D11j geprüft, D11 insgesamt weiterhin `in-progress`.**
D12/D13 und Meridian-Abnahme L07 bleiben offen. Prüfbasis dieses Blocks: `cef3feb`.
D11j wird gemeinsam mit Belegen, Changelog und dieser Fortsetzungsdoku auf `main`
committed. Den Abschlusscommit über `git log -1 --oneline` feststellen.
Kein Push, kein Release-Tag, keine Terra-Übergabe.

## D11j

- `NativeSketchConstraints` bildet alle 26 bestehenden Constraintarten ab.
- Constraint-IDs, Reihenfolge, erste/optionale zweite/dritte Elementreferenz,
  Punktcodes, getrennte Maß-/Radiantwerte und Zusatzparameter bleiben erhalten.
- Echte OCAF-Verweise dürfen nur auf Elemente derselben Skizze zeigen.
- Fixierungen und Support-Koordinaten enthalten vollständige Zusatzwerte.
- Lesbarkeit ist unabhängig von Lösbarkeit: Widersprüche werden nicht repariert.
  Lösbare und widersprüchliche Skizzen haben nach Save/Open identische Solverresultate.
- Gesamtbudget zählt Features, Elemente und Constraints gemeinsam.
- `Primitives.v1` und bisherige Felder bleiben erhalten; D11i-Dateien sind lesbar.
- Extrusionen werden weiterhin vor nativen Schreibmutationen abgewiesen.

**63/63 Windows**, **59/59 FreeBSD/Sway**; native Constraintdateien in beiden
Richtungen samt Rückschreiben/-lesen bestanden, alle vier Dateien byteidentisch.
**288 Quellhashes identisch**. Alle Builds und Tests beendet.
[Evidenz und Artefakte](implementation-plan/evidence/D11j.md).

## Nächster Block

Native **Extrusionen mit Profilreferenzen**: alle vorhandenen Parameter, IDs und
Eingaben verlustfrei abbilden. Featurefamilien einzeln gegen immutable Snapshots
prüfen. Die vorhandenen nativen Kopf-/Box-/Zylinder-/Skizzen-/Constraintregressionen
erhalten. Strukturell gültige, aktuell nicht auswertbare Zustände nicht still ändern.

Erst nach vollständiger Abbildung folgt der Autoritäts-/Store-Wechsel gemäß
[ADR 0008](adr/0008-ocaf-binary-persistence.md) und D12/D13. Noch kein produktiver
nativer Dispatch: **VOLUND 16 bleibt Produktwriter**. Die Datei enthält noch keine
produktive persistente DocumentId. Kein paralleler mutabler OCAF-Schattenzustand.

## Prüfumgebung

- Windows: `tools/setup-windows.ps1 -SkipConfigure -PassThru` im selben Prozess wie
  Build/Test aufrufen; Buildverzeichnis `build/p02-qt-final`.
- FreeBSD: `eduard@192.168.1.238`, isolierter Baum
  `/home/eduard/src/voelund-p04-20260913`.
- OCCT-Prefix: `<Prüfbaum>/build/d11d-occt/install`; `OpenCASCADE_DIR` ausdrücklich
  auf `<Prefix>/lib/cmake/opencascade` setzen. Das `.tools/occt-freebsd`-Symlinkziel
  bleibt die ursprüngliche gemeinsame Installation, nicht der Prüf-Prefix.
- Acht D11j-Quelldateien und Prüfskript wurden nach ausdrücklicher Nutzerfreigabe
  übertragen. Keine Quelldateien gelöscht. Alle 16 OCCT-Laufzeitauflösungen des
  Constrainttests zeigen auf den isolierten Prefix.
- Befehle, Quelllisten, Hashes, Dateirundläufe und Logs stehen in `D11j-artifacts`.

## Versionierung

Basisversion `0.1.0`, Changelog unter `Unreleased` aktuell. Produktversion,
Anwendungsschema und OCAF-Containerformat 12 bleiben getrennt.
`cef3feb` enthält den vorherigen Abschluss D11i.
[Vortagsnotiz](session-checkpoint-2026-09-15.md) bleibt historischer Nachweis.
