# UNDO-MESSUNG – Speicher und Zeit des Undo-Stapels

**Status:** abgenommen am 23.10.2026. Messung am Stand `4eaf1d0`, Code `fdf2a99`; CI-Lauf 723
grün; `main` auf Gitea und GitHub. Es gibt kein Delta-Undo und keine Änderung am
Produktverhalten.

## Auftrag

Speicher und Zeit des Undo-Stapels messen: vollständige OCAF-Stände, D12c. Kleines, mittleres
und großes Testmodell, je 10, 100 und 500 Schritte. Ergebnis und Einschätzung, ab welcher
Modellgröße es kritisch wird, gehen nach `docs/performance.md`.

## Messaufbau

- `tests/perf/UndoBench.cpp` (`voelund_undo_bench <modell> <schritte> [<STEP-Datei>]`):
  - je Lauf ein Prozess;
  - das Modell wird gebaut, gespeichert und neu geöffnet, damit die Historie leer beginnt;
  - jeder Schritt ist eine übernommene Änderung des ersten Quaders;
  - gemessen werden die Zeiten (Median, p95, Maximum) für Ändern, alle Undo und alle Redo,
    dazu der Zuwachs des belegten Speichers;
  - bei 8 GiB Speicher bricht ein Lauf ab und meldet das (kam nicht vor).
- `tools/run-undo-bench.sh` führt die neun Läufe aus und schreibt einen Bericht mit Rechner und
  Stand.
- CTest hält nur den kleinsten Lauf lauffähig (`perf.smoke.undo-small-10`) und misst keine
  Zeiten.
- Modelle:
  - klein: ein Quader;
  - mittel: 25 Körper aus Quader und vereinigtem Zylinder, 49 Features;
  - groß: mittel plus das NIST-Teil `nist_stc_08_asme1_ap242-e3.stp` als eingebetteter
    STEP-Import, 50 Features, Datei 300 KiB.
- Rechner: Fedora 44, i7-7500U, 15,5 GiB, Release-Build, ohne CI-Lauf.

## Ergebnis

Die Tabelle und die Einschätzung stehen in `docs/performance.md` (Abschnitt „Undo-Stapel“), die
Rohwerte in `UNDO-artifacts/undo-2026-10-23.txt`. Kurz:

- **Speicher:** Je Schritt etwa die Größe des gespeicherten Dokuments, rund 1,1-fach, linear und
  ohne Obergrenze. Beim großen Modell sind es 0,34 MiB je Schritt, nach 500 Schritten 170 MiB.
- **Zeit:** rund 1 ms je KiB Dokument je Undo oder Redo, unabhängig von der Länge der Historie
  (klein 2 ms, mittel 20 ms, groß 0,34 s).
- **Kritisch:**
  - ab etwa 2 MiB Dokumentgröße: Undo über 2 s, 500 Schritte über 1 GiB;
  - deutlich bei großen eingebetteten STEP-Importen (10 MiB und mehr).
  - Dann bräuchte es eine Obergrenze der Historie oder Delta-Undo, eine eigene Entscheidung.

## Grenzen

- Ein Rechner, ein Änderungsmuster (Quadermaß). Änderungen, die mehr neu auswerten, verschieben
  die Zeiten nach oben, nicht den Speicher je Schritt.
- Die Spitzen-Belegung beim Kodieren eines Stands ist nicht getrennt ausgewiesen; gemessen ist
  der bleibende Zuwachs.
