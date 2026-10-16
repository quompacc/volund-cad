# Fortsetzung – 16. Oktober 2026 (Pause nach G3)

**Einstieg für die nächste Sitzung:** Dieses Dokument zuerst lesen.

- Arbeitsregeln, Werkzeuge und Zugänge: [session-checkpoint-2026-10-04.md](session-checkpoint-2026-10-04.md).
- Ergänzungen dazu: [session-checkpoint-2026-10-08.md](session-checkpoint-2026-10-08.md).
- Unten stehen nur die Ergänzungen seit dem 13.10.

## Stand des Repos

- Branch `ux01`. `main` steht auf `c6df946` (CI-Lauf 639 grün); `ux01` ist gleich.
- **Letztes Paket:** M18 (Code `9ecd22c`, CI 636, Abnahme `afa0206`). Damit ist G3 bis auf T05
  erreicht, siehe [G3-Bericht](implementation-plan/evidence/G3.md).
- **Handbuch:** [handbuch.md](handbuch.md) beschreibt Umfang und Grenzen von v1.
- **Tests:**
  - Fedora: 134 Tests ohne Desktop, 71 Desktop-Tests unter virtuellem Wayland
    (`sh ~/bin/voelund-desktop.sh "desktop\."`).
  - Windows: bauen und `ctest -LE desktop` (134).
- **Schema der Dokumente:** 35. Verlauf in `docs/versioning.md`; neu seit dem 13.10.:
  - 18–30: Werkzeugpakete;
  - 31: Spiegeln; 32: Muster; 33: Bewegen;
  - 34: Unterdrücken und Rückrollmarke;
  - 35: STEP-Import.
- **Installiert:** Windows-Build in `Installer/` (Start: `Installer/bin/voelund-cad.exe`), Stand M18.

## Erledigt seit 13.10.2026

| Paket | Inhalt | Nachweis |
|---|---|---|
| Werkzeugpakete (14.10.) | S15–S21 (Punkt, Radiusmaß, Kanten projizieren, Referenzmaße, Skizzenfase, Drehen/Skalieren, B-Spline), M19–M27 (Bezugsebene/-achse, Drehen/Nut, Kombinieren, Wandstärke, Folgen, Ausformung, Formschräge, Wendel, weitere Grundkörper) | Nachweise je Paket |
| U14 (14.10.) | Viewport-first-Shell: schwebende Karten, gruppierte Werkzeugleiste, Timeline unten Mitte | [U14](implementation-plan/evidence/U14.md) |
| M09 (15.10.) | Körper spiegeln (Grundebene, Bezugsebene, ebene Fläche), neuer Körper oder vereinigt | [M09](implementation-plan/evidence/M09.md) |
| M10 (15.10.) | Körpermuster linear/kreisförmig, Anzahl mit Original, neue Körper oder vereinigt | [M10](implementation-plan/evidence/M10.md) |
| M11 (15.10.) | Jeden Körper bewegen und drehen; „Bewegen N“ in der Historie | [M11](implementation-plan/evidence/M11.md) |
| M12 (15.10.) | Messen mit zwei Klicks (Länge, Radius, Fläche, Volumen, Abstand, Winkel), Kopieren | [M12](implementation-plan/evidence/M12.md) |
| M13 (16.10.) | Historische Features bearbeiten; „Mit Folgefehlern übernehmen“ in allen Panels | [M13](implementation-plan/evidence/M13.md) |
| M14 (16.10.) | Unterdrücken/Aktivieren, Rückrollmarke wie in Fusion (kein Rückgängig-Schritt) | [M14](implementation-plan/evidence/M14.md) |
| M15 (16.10.) | Timeline: Marke ziehen, Tastaturbedienung | [M15](implementation-plan/evidence/M15.md) |
| M16 (16.10.) | Schritte per Ziehen umordnen; Löschen mit Dialog (mit Abhängigen / nur dieses / Abbrechen) | [M16](implementation-plan/evidence/M16.md) |
| M17 (16.10.) | STEP lesen/schreiben im Kern; Importfeature mit eingebetteten Solids | [M17](implementation-plan/evidence/M17.md) |
| M18 (16.10.) | STEP im Datei-Menü (Strg+I/Strg+E, Öffnen nimmt .step); A-PART-01 per Klick durch die Shell; A-PART-02; Handbuch; G3-Bericht | [M18](implementation-plan/evidence/M18.md), [G3](implementation-plan/evidence/G3.md) |

## Nutzerentscheidungen dieses Blocks

Die Einzelheiten stehen jeweils im Nachweis unter „Entscheidungen“. Kurz:

- **M09/M10:** Ergebnis als neue Körper oder vereinigt. Achsen wie beim Drehen. Die Anzahl
  zählt das Original mit.
- **M11:** „Bewegen N“ als Timeline-Feature. Grundkörper behalten ihre eigene Platzierung,
  solange sie das letzte Feature ihres Körpers sind.
- **M12:** Messen-Panel mit zwei Klicks, drei Nachkommastellen, Kopieren.
- **M14:**
  - Unterdrücken reicht den Körper durch.
  - Marke wie in Fusion.
  - Bedienung über Kontextleiste und Timeline-Menü.
- **M16:** Umordnen per Ziehen eines Schritts; Löschen mit Dialog, der die Abhängigen nennt.
- **M17:**
  - Ein Importfeature, je Solid ein Körper.
  - Export: Auswahl, sonst alle sichtbaren Körper.
  - Schema 35.
- **M18:**
  - Datei-Menü statt Werkzeugleiste.
  - Import ins offene Dokument; Öffnen legt für .step ein neues Dokument an.
  - Rückmeldung in der Statuszeile, Dialog nur bei Problemen.
- **Unverändert:**
  - Abnahmeregel vom 08.10.: vorab fragen nur bei neuem Bedienkonzept, Abweichung von ADR 0012
    oder Gespeichertem.
  - Aufnahmen nach `artifacts/<paket>-review/`.

## Nächste freie Pakete (Nutzer entscheidet)

Bereit sind nur noch drei Pakete:

- **T05** – Koplanare Flächen nach Drücken/Ziehen verschmelzen. Das ist das letzte offene
  G3-Paket. Laut Nutzer „später“; nicht ohne Rückfrage beginnen.
- ~~**PAR02**~~ – Parametertabelle in der Oberfläche: am 16.10.2026 nach der Pause umgesetzt und
  abgenommen (`618f685`, CI 646); G2 ist damit geschlossen. Siehe [PAR02](implementation-plan/evidence/PAR02.md).
- **A02** – Verbindlichen Prüfumfang definieren. Dafür braucht es erst Designarbeit.

Die G4-Pakete (L-Pakete) hängen an diesen. Beim Wiedereinstieg den Nutzer fragen, womit es
weitergeht.

## Offene Punkte

- **CI-Runner umziehen** (Fedora-Laptop → Fedora-44-VM auf Proxmox): weiter offen, der
  Nutzer entscheidet. **Am Runner nichts ändern.**
- **Windows:** `platform.atomic_file_store` lief am 16.10. unter Last erst im dritten Versuch
  durch (23 s bei 30 s Grenze, langsame Dateizugriffe). Beobachten; er hat keinen Bezug zu G3.
- **A-PART-02:** Die Radiusänderung ist nur im Anwendungskern geprüft
  (`application.reference_ring`), nicht per Klick in der Shell.
- **Grenzen der neuen Pakete** stehen gesammelt im [Handbuch](handbuch.md). Die wichtigsten:
  - Importkanten lassen sich nicht projizieren.
  - Musterkopien und Spiegelhälften haben keine eigenen Kantennamen.
  - Umordnen und Löschen gehen nur für einen Schritt.
- **Unverändert offen:**
  - Spiegelzellen per RegionId.
  - GitHub-Spiegel nicht aktuell (nur auf Wunsch).
  - Ursache von CI-Lauf 453.

## Ergänzungen zu Werkzeugen und Fallstricken

- **OCCT:**
  - Das schlanke Linux-OCCT enthält jetzt `TKDESTEP` und `TKXCAF` (M17).
  - `tools/setup-linux.sh` baut einen älteren Cache inkrementell nach; das ist in der CI
    einmal geschehen.
  - Das Fedora-Repro-OCCT `~/repro/occt-m05` ist ebenfalls nachgebaut (Log
    `~/repro/occt-m17.log`).
- **OCCT 7.9 (Linux) gegen 8.0 (Windows):**
  - Unter 8.0 sind veraltete Typedefs Fehler (`TDF_LabelSequence`,
    `TColStd_SequenceOfAsciiString`, `TopTools_IndexedMapOfShape`); `NCollection_*` direkt
    verwenden.
  - `NCollection_HArray1<T>` ist unter 7.9 kein Template; den Typ aus dem Attribut ableiten
    (siehe `NativeImport.cpp`).
  - `Standard_Failure::GetMessageString` ist veraltet → `occtFailureDiagnostic`.
- **Native Datei:** Der Leser lässt nur bekannte OCAF-Attributtypen mit exakt geprüftem
  Binärformat zu (`NativeAttributeValues.cpp`). Ein neuer Attributtyp muss dort eingetragen
  werden.
- **Quelltextgrenze:** höchstens 600 Zeilen je Datei (`architecture.source_limits`).
  `CadSession.hpp` liegt knapp darunter; neue Typen in eigene Header (Beispiel
  `StepExchangeResult.hpp`).
- **Windows-Makro `near`:** Funktionen in Tests nicht `near` nennen, wenn Qt- und
  Windows-Header zusammenkommen (`closeTo`).
- **Bash-Heredocs** machen aus einem doppelten Backslash einen einfachen. Ein Python-Skript mit
  `'\\0'` schrieb so ein echtes NUL-Byte in C++-Quelltext. Edit-Skripte mit Backslashes mit dem
  Write-Werkzeug anlegen oder solche Zeilen mit dem Edit-Werkzeug schreiben.
- **Fedora:**
  - Keine Desktop-Tests, während dort ein CI-Lauf aktiv ist (Fensteraktivierung, CI 623).
  - Ein Push auf `main` startet ebenfalls einen CI-Lauf.
