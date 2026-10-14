# Fortsetzung – 8. Oktober 2026 (Abend)

**Einstieg für die nächste Sitzung:** Dieses Dokument zuerst lesen. Arbeitsregeln,
Werkzeuge und Zugänge stehen in
[session-checkpoint-2026-10-04.md](session-checkpoint-2026-10-04.md); unten sind nur
Ergänzungen aufgeführt. Der Stand vom Morgen (S01, S02, L05, T04) steht in der
Git-Historie dieser Datei.

## Stand des Repos

- Branch `ux01`. `main` folgt nach jeder grünen Abnahme per Fast-Forward.
- Letztes Paket: M08 (Code `d872d05`; CI-Lauf 563). Danach Korrekturen nach Nutzertests
  vom 13.10. (`5a47096` bis `dd89158`, siehe
  [Nachtrag](implementation-plan/evidence/M05-M08-Nachtrag-2026-10-13.md)). Der
  Nachtrags-Commit enthält diese Datei. Vor dem Weiterarbeiten prüfen, ob `main` auf ihm steht; sonst seinen CI-Lauf
  prüfen und `main` nachziehen (siehe Arbeitsregeln).
- **Tests:** Fedora (GCC, ohne Desktop) 112; Desktop unter virtuellem Wayland 49 (Lauf `^desktop`). **Windows:
  nur bauen**, keine Desktop-Tests auf dem Rechner des Nutzers (Nutzerwunsch 10.10.2026:
  aufblitzende Fenster stören). Desktop-Tests auf Fedora (Skript `~/bin/voelund-desktop.sh` auf Fedora, wie `~/repro-snap.sh`). Desktop-Tests laufen in der
  CI.
- **Schema der Dokumente:** 17 (17: benannte Rundungs-/Fasenflächen und Kanten-IDs; 16: Bohrung; 15: Fase; 14: Verrundung; 13: symmetrische Extrusion, Schneiden; Verlauf in `docs/versioning.md`).

## Erledigt am 08.10.2026 (zweiter Block)

| Paket | Inhalt | Nachweis |
|---|---|---|
| S03 | Ein Startweg für den Skizzeneditor; Ansicht nach Skizzenrahmen; aufrechte Flächenskizzen; Rückkehrkamera nach OK/Abbrechen; gedrehte Grundformen behalten die Flächenorientierung | [S03](implementation-plan/evidence/S03.md) |
| S08 | Eine starre Transformation für alle Skizzengeometrien; Spiegelachse als Wert; Alt-Rechtecke werden bei schräger Lage vier gebundene Linien; Verschieben/Löschen der ganzen Auswahl | [S08](implementation-plan/evidence/S08.md) |
| S04 | Fangmatrix für Linie/Polylinie/Rechteck; keine überflüssigen Beziehungen; Schalter „Beziehungen: Automatisch / Keine“ im Skizzenpanel | [S04](implementation-plan/evidence/S04.md) |
| S05 | Kurvenbeziehungen nur für unterstützte Paare; keine angekündigte Tangente am Bogenende auf einer Linie; entartete Picks mit genauer Meldung | [S05](implementation-plan/evidence/S05.md) |
| S09 | Exakter Versatz für Elemente und Konturen, Vorschau = Übernahme, Ellipse als nicht unterstützt gemeldet | [S09](implementation-plan/evidence/S09.md) |
| Nachtrag (13.10.) | Nach Nutzertests: Kanten vorwählen (Strg ergänzt), Greifer für Radius/Abstand, Karten wachsen mit dem Text, Fase durch angrenzende Rundung, Kantenringe um behandelte Ecken (auch 2–3 mm), benannte Rundungs-/Fasenflächen (Schema 17). Grenze: kein Überlauf großer Rundungen („Kissen“ wie in Fusion) – OCCT-Grenze | [Nachtrag](implementation-plan/evidence/M05-M08-Nachtrag-2026-10-13.md) |
| M08 (13.10.) | Bohrung (Schema 16): Klick auf ebene Fläche, X/Y im Flächenrahmen, Durchmesser, Tiefe oder durchgehend; weitere Klicks versetzen sie (auch über die Vorschau); A-HOLE-01. Dazu Fix M06/M07: weitere Kanten über die Vorschau wählbar (`5a47096`) | [M08](implementation-plan/evidence/M08.md) |
| M07 (12.10.) | Fase als eigenes Feature „Fase N“ (Schema 15): Flächenpaar-Kanten, symmetrischer Abstand, eigener Befehl neben „Verrunden“ im selben Panel; Kernel/Auswertung mit der Verrundung geteilt | [M07](implementation-plan/evidence/M07.md) |
| M06 (12.10.) | Verrunden im Werkzeugpanel: Kanten im Ansichtsfenster anklicken (gewählte orange, wieder anklickbar), Radius, Vorschau aus derselben Auswertung, abgelehnter Radius erklärt mit roten Kanten, OK/Abbrechen, Bearbeiten über Timeline; A-UI-02 für die Verrundung | [M06](implementation-plan/evidence/M06.md) |
| M05 (12.10.) | Verrundung als Feature (Schema 14): Kante als Paar benannter Flächen, ein Radius, mehrere Kanten; Kernel mit Historie und Rückführung des Fehlers auf Kanten; A-FILLET-01; Linux-OCCT um `TKFillet` ergänzt | [M05](implementation-plan/evidence/M05.md) |
| M04 (12.10.) | Produktives Extrusionspanel: Feldregeln, Ziel im Hinweis, Bearbeiten öffnet das Panel wieder (Timeline/„Bearbeiten“), Bereiche per Klick umschalten, ungültige Bereiche deutsch erklärt und rot markiert; A-UI-01 vollständig in `desktop.ui_flow`; G2 protokolliert (PAR02 offen) | [M04](implementation-plan/evidence/M04.md), [G2](implementation-plan/evidence/G2.md) |
| M03 (12.10.) | Vereinigen/Abziehen/Schneiden mit festem Vertrag: abgelehnte Fälle mit Code (getrennt, kein Material, ganzer Körper, leerer Schnitt), Körper und Bereich, deutsch erklärt im Panel; A-EXT-02 | [M03](implementation-plan/evidence/M03.md) |
| M02 (11.10.) | Ein geprüftes Prisma in der Kernel-Schicht (`kernel::Prism`); symmetrische Extrusion (Distanz = Gesamtlänge) und „Schneiden“ in Modell, Panel und MCP, Schema 13; A-EXT-01 auf XY/XZ/YZ | [M02](implementation-plan/evidence/M02.md) |
| M01 (10.10.) | Mehrere unabhängige Körper durch die Shell: Zylinder auf gewähltem Körper, sonst neuer Körper; Körpernamen (umbenennbar, ein Rückgängig-Schritt) und Sichtbarkeit (gespeichert, ohne Rückgängig-Schritt) im Dokument, Schema 12; Körperzeile im Baum wählt den Körper; A-BODY-01/03, A-DAG-01 | [M01](implementation-plan/evidence/M01.md) |
| S14 (10.10.) | Matrix je Werkzeug/Operation (`sketch_tool_matrix`), Referenzskizze durch die Shell, Ziehen nach A-TXN-03 mit Maus; fünf gefundene Fehler behoben (Muster auflösen + speichern, Bereich auf Fläche wählen, Editor unter Körpern, Element mit Maus ziehen, Escape am Manipulator); Grenzen gesammelt in sketch-data-model.md. Dazu: Dreipunktbogen-Reihenfolge, Skizze auf Extrusionsdeckfläche, Werkzeugleiste in Familien | [S14](implementation-plan/evidence/S14.md) |
| U12 (09.10.) | Ein Composition Root (`startProduct`) für Produkt und UI-Tests; Installation trägt die getestete Laufzeit (Windows-DLLs, Linux-OCCT mit `$ORIGIN`), `desktop.installed_product` startet die installierte Datei; ungenutzte APIs entfernt; README auf den Produkteinstieg | [U12](implementation-plan/evidence/U12.md) |
| U11 (09.10.) | Echte UI-Abläufe als Tests (Skizze, Panels, Dokument über Dateimenü, Gegenprobe abgetrennter Handler); Bildprüfung: FreeType unter Windows, Beschriftungen neben dem Punkt, Einheiten, Skizze in freier Fläche, schmale Kopfzeile, Extrusionsvorschau | [U11](implementation-plan/evidence/U11.md) |
| S13 (09.10.) | Escape schrittweise ohne Verwerfen, Abbrechen fragt, Enter/Strg+Enter; Rahmen Fenster/Kreuzen; Element vor Beschriftung; vier Ansichtsschalter als Befehle | [S13](implementation-plan/evidence/S13.md) |
| S12 (09.10.) | Assoziative Skizzenmuster (linear, Kreis, Spiegelung), Schema 11; Kopien folgen dem Original, schreibgeschützt; Muster ändern/auflösen | [S12](implementation-plan/evidence/S12.md) |
| S11 (09.10.) | Verrundung mit virtueller Ecke: Eckmaße bleiben, Bestimmtheit bleibt; abgewiesene Linien rot mit größtem Radius | [S11](implementation-plan/evidence/S11.md) |
| S07 (09.10.) | Eine Beschriftungsprojektion für alle Bedingungsarten; überflüssige Bedingungen gemeldet; Maß ändern als geprüfter Befehl, gebundene Maße verweisen auf den Parameter | [S07](implementation-plan/evidence/S07.md) |
| S06 (09.10.) | Polygon regelmäßig über Hilfskreis (Umkreisradius, Mittelpunkt bindet), Langloch mit Hilfsachse (Länge, Ausrichtung); Endpunkt-Tangente im Löser | [S06](implementation-plan/evidence/S06.md) |
| S10 | Stutzen/Verlängern/Teilen für Linie, Bogen (Stutzen auch Kreis); Stück zwischen zwei Schnitten; entfernte Bedingungen in der Statuszeile; neue Enden an die Schnittkante gebunden | [S10](implementation-plan/evidence/S10.md) |

## Nutzerentscheidungen dieses Blocks

- **Abnahmeregel (08.10.2026, nach S03):**
  - Vorab fragen nur bei neuem Bedienkonzept, sichtbarer Abweichung von ADR 0012 oder
    Gespeichertem (Schema, Bindungen).
  - Sonst selbst abnehmen, sobald die CI grün ist, und sichtbare Änderungen kurz im Bericht
    nennen. Aufnahmen nur, wenn wirklich etwas zu sehen ist.
  - Hintergrund: Der eigentliche Prüfstein ist ein echtes Teil im Produkt. Viele Punkte,
    auch Abläufe, die Aufnahmen nicht zeigen, fallen erst dort auf und werden dann geändert.
- **Aufnahmen für den Nutzer** gehören nach `artifacts/<paket>-review/` im Repo
  (gitignored). Links in den Scratchpad lassen sich bei ihm nicht öffnen.
- Unverändert: Reihenfolge nach Plan; M12 (Messen) und M17 (STEP) nicht vorziehen.

## Nächste freie Pakete

- **Werkzeugpakete vom 14.10.2026 (vor M09):** Abgleich mit FreeCAD 1.1, Nutzer: Gruppe 1 und 2 aufnehmen und ohne Rückfragen umsetzen. Reihenfolge S15, S16, M21, M19, M20, M22, S17, S18, S19, S20, M27, M23, M24, M26, M25, S21 (`packets.json`). Pakete in Kapitel 04/05 unter „Ergänzung vom 14.10.2026“. Verrundung bleibt beim Maschinenbau-Standardsatz (13.10.); freie Formen über Drehen (M20) und Folgen (M23).
- **M09** Parametrische Körperspiegelung (G3). Nächstes Paket nach Plan. Verrundung, Fase
  und Bohrung sind fertig (M05–M08, Schema 16); ihre Entscheidungen stehen in
  [M05](implementation-plan/evidence/M05.md), [M07](implementation-plan/evidence/M07.md) und
  [M08](implementation-plan/evidence/M08.md). Entscheidungen zu M05 (Kante als Flächenpaar, ein Feature mit Kantenliste,
  Schema 14) stehen in [M05](implementation-plan/evidence/M05.md). G2 ist bis auf PAR02 erreicht ([G2](implementation-plan/evidence/G2.md)). M01 und M02 sind abgenommen; ihre Entscheidungen stehen in den
  Nachweisen [M01](implementation-plan/evidence/M01.md) und
  [M02](implementation-plan/evidence/M02.md).
- **PAR02** Parametertabelle in der Oberfläche (G2), frei.
- **T05** Koplanare Flächen verschmelzen (G3). Laut Nutzer „später“; nicht ohne Rückfrage.
- **A02** braucht weiterhin Designarbeit.

## Offene Punkte

- **CI-Runner umziehen** (Fedora-Laptop → Fedora-44-VM auf Proxmox): weiter offen, der
  Nutzer entscheidet. **Am Runner nichts ändern.**
- **Aus diesem Block bekannte Grenzen:**
  - Tangente im Endpunkt eines Bogens an eine Linie: Der Löser kann sie seit S06
    (Winkel am gemeinsamen Punkt); beim Zeichnen wird sie weiter nicht angeboten (S05).
  - Versatz einer Kontur mit Knick zwischen Linie und Bogen wird abgewiesen (S09).
  - Die Vorschau kündigt „waagrecht“ auch an, wenn die Beziehung als überflüssig entfällt;
    die Linie bleibt über die Fänge waagrecht (S04).
  - `sketch_on_face` über MCP nutzt den Flächenrahmen, nicht den aufrechten Rahmen (S03).
  - Ellipsen schneiden beim Stutzen/Verlängern nicht; ein Ende im Inneren eines Bogens wird
    nicht gebunden, weil es „Punkt auf Bogen“ im Format nicht gibt (S10).
- **Unverändert offen:** Spiegelzellen per RegionId, PAR02 (Parameter-UI), GitHub-Spiegel
  nicht aktuell (nur auf Wunsch), Ursache von CI-Lauf 453.

## Ergänzungen zu Werkzeugen und Fallstricken

- **Sanitizer:** MSVC AddressSanitizer funktioniert in einem eigenen Build-Verzeichnis:
  `cmake --preset windows-msvc -B build/s08-asan -DVOELUND_BUILD_DESKTOP=OFF
  "-DCMAKE_CXX_FLAGS_DEBUG=/Zi /Ob0 /Od /fsanitize=address"` (ohne `/RTC1`), dann nur das
  Testziel bauen; zur Laufzeit muss
  `VC\Tools\MSVC\14.51.36231\bin\Hostx64\x64` im `PATH` stehen. Auf Fedora fehlt
  `libasan`; nicht ohne Rückfrage installieren.
- **clang-format** formatiert ganze Dateien um. Nur die eigenen Zeilen behalten,
  Fremdänderungen zurücknehmen.
- **GCC vor dem Push** findet, was MSVC durchlässt, z. B. ungenutzte Variablen
  (`-Werror=unused-variable`, S09).
- **Löser:** PlaneGCS meldet widerspruchsfreie, überflüssige Bedingungen nicht als
  Konflikt. Tests prüfen deshalb, dass jede neue Bedingung einen Freiheitsgrad wegnimmt
  (`requireEffective` in `SketchSnapMatrixTests.cpp`).
- **`\n` in C++-Strings** aus Bash-Heredoc mit eingebettetem Python wird weiterhin zum
  echten Zeilenumbruch. Solche Zeilen mit dem Edit-Werkzeug schreiben.
