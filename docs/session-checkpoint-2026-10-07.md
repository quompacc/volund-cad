# Fortsetzung – 7. Oktober 2026

> **Abgelöst durch [session-checkpoint-2026-10-08.md](session-checkpoint-2026-10-08.md).**

**Einstieg für die nächste Sitzung:** Dieses Dokument zuerst lesen. Arbeitsregeln,
Werkzeuge und Zugänge stehen unverändert in
[session-checkpoint-2026-10-04.md](session-checkpoint-2026-10-04.md).

Der Auftrag aus [session-checkpoint-2026-10-06.md](session-checkpoint-2026-10-06.md) ist
erledigt: zuerst Block A (Werkzeuglücken aus AI03c), dann Block B (U07–U10).

## Stand des Repos

- Branch `ux01`. `main` steht auf dem Abnahme-Commit von U10 (siehe `git log origin/main`).
- Windows: 125/125 Tests. Fedora (GCC): 91/91 ohne Desktop; die Desktop-Tests laufen in
  der CI.

## Erledigt

| Paket | Inhalt | Nachweis |
|---|---|---|
| T01 | Skizze auf Körperfläche über MCP (`create_sketch_on_face`, Flächen in `get_context`) | [T01](implementation-plan/evidence/T01.md) |
| T02 | Ergebnis der Extrusion (neuer Körper / vereinigen / abziehen) und Richtung; Schema 8 | [T02](implementation-plan/evidence/T02.md) |
| T03 | Startversatz und Extrusion bis Fläche; Schema 9 | [T03](implementation-plan/evidence/T03.md) |
| U07 | Dateiargument über den sicheren Öffnen-Weg; Dokumentschutz im echten Fenster getestet | [U07](implementation-plan/evidence/U07.md) |
| U08 | Ein Weg zur Skizzen-Erzeugung in der Anwendungsschicht, mit Rollen | [U08](implementation-plan/evidence/U08.md) |
| U09 | Ziehen und getippte Verschiebung als ein Undo-Schritt; Escape bricht nur das Ziehen ab | [U09](implementation-plan/evidence/U09.md) |
| U10 | Fehlerkarte, Marker in Baum und Timeline, Hinweisband für veraltete Geometrie | [U10](implementation-plan/evidence/U10.md) |

Nutzerentscheidungen dieser Arbeit; Details in
[T-plan.md](implementation-plan/T-plan.md) und `packets.json`:

- Block A in der Reihenfolge erst Fläche, dann Operation. Das Verschieben von Körpern
  bleibt in M11.
- T02-Panel: „Richtung + Ergebnis“.
- T03: „Versatz + Bis Fläche“.
- U10: Fehlerkarte mit Markern; veraltete Geometrie mit Hinweisband und blassen Körpern.
- Der Nutzer hat alle Aufnahmen gesehen (T02, T03, U10).

Die Abnahme mit echtem Client (`voelund_assistant_panel_acceptance`) besteht 8/8 auf dem
Stand mit T01–T03. Die Rippe ist jetzt ein Körper mit der Platte.

## Nächste freie Pakete (nach Abhängigkeiten)

- **S01** Semantischen SketchCreationResult abschließen. U08 hat mit
  `SketchCreationResult` und den Rollen die Grundlage gelegt.
- **S02** Editorzustand bei Solve/Accept/Cancel vollständig sichern.
- **L05** Autosave (G4).
- **A02** braucht weiterhin Designarbeit.

U11 (automatischer Test des ganzen UI-Ablaufs) wartet zusätzlich auf S03/S07.

## Offene Punkte (unverändert)

- Spiegelzellen per RegionId (Nutzerentscheidung nötig).
- PAR02 (Parameter-UI).
- GitHub-Spiegel nicht aktuell; nur auf Wunsch synchronisieren.

## Nachtrag 7. Oktober 2026 (abends)

Die Nutzerfreigabe lautete „S01, S02 und L05 frei“. Alle drei sind abgenommen:

| Paket | Inhalt | Nachweis |
|---|---|---|
| S01 | Erzeugungsergebnis mit Punktrollen, Bogenrichtung, Polygonindex und erzeugten Bedingungen; unpassender Fang ist ein Fehler | [S01](implementation-plan/evidence/S01.md) |
| S02 | Ein Editorzustand für Undo, Ziehen und Rollback; `canAccept` mit expliziten Zuständen | [S02](implementation-plan/evidence/S02.md) |
| L05 | Autosave und Wiederherstellung mit Startdialog; Skizzenentwurf getrennt gesichert (L05a + L05b) | [L05](implementation-plan/evidence/L05.md) |

**Nutzerentscheidung L05 (07.10.2026):**
- Dialog beim Start.
- Sicherung 30 s nach der letzten Änderung, spätestens alle 2 min.
- Ein offener Skizzenentwurf wird getrennt gesichert und im Editor wiederhergestellt.

Windows hat 127 Tests (neu: `application.sketch_editor_state`, `desktop.recovery`).
Fenstertests setzen `VOELUND_RECOVERY_DIR` über `IsolatedSettings.hpp`.

**Jetzt frei (Abhängigkeiten erfüllt):**
- **S03** Skizze aus leerem Dokument auf Basisebene.
- **S08** Transformationen und Mehrfachauswahl.
- **A02** braucht weiterhin Designarbeit.

**Offen zur Entscheidung durch den Nutzer** (Gespräch 07.10.2026):
- **Reihenfolge Richtung „produktiv“:** Messen (M12) und STEP (M17) früher? Die Kette
  dorthin ist M11 → M12 → M17. Abrunden (M05) hängt an M03.
- **Vorschlag „Drücken/Ziehen auf Körperflächen“:** Push/Pull im Stil von SketchUp, das
  ein normales Extrusions-Feature erzeugt. Das ist eine sichtbare Ergänzung zu ADR 0012
  und braucht Freigabe. Anlass war der Vergleich mit IngeTrazo, einem SketchUp-ähnlichen
  Modellierer ohne Parametrik und ohne Volumenkernel.
