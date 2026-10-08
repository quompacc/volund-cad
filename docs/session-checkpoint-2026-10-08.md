# Fortsetzung – 8. Oktober 2026

**Einstieg für die nächste Sitzung:** Dieses Dokument zuerst lesen. Arbeitsregeln,
Werkzeuge und Zugänge stehen in
[session-checkpoint-2026-10-04.md](session-checkpoint-2026-10-04.md); unten sind nur
Ergänzungen aufgeführt.

## Stand des Repos

- Branch `ux01`. `main` steht auf `0df0be5` (Abnahme T04). `ux01` ist identisch, der
  Arbeitsbaum ist sauber.
- **Windows:** 129 Tests. **Fedora (GCC):** 93 ohne Desktop. Die Desktop-Tests laufen in
  der CI. Letzter grüner Lauf: 464.
- **Schema der Dokumente: 10** (T04).

## Erledigt seit dem 07.10.2026

| Paket | Inhalt | Nachweis |
|---|---|---|
| S01 | Erzeugungsergebnis mit Punktrollen, Bogenrichtung, Polygonindex und erzeugten Bedingungen; unpassender Fang ist ein Fehler | [S01](implementation-plan/evidence/S01.md) |
| S02 | Ein Editorzustand für Undo, Ziehen und Rollback; `canAccept` mit expliziten Zuständen | [S02](implementation-plan/evidence/S02.md) |
| L05 | Autosave und Wiederherstellung mit Startdialog; Skizzenentwurf getrennt gesichert | [L05](implementation-plan/evidence/L05.md) |
| T04 | Drücken/Ziehen auf Körperflächen: Fläche als Profil, stabile Boden- und Seitenflächen, Pfeil + Maßfeld | [T04](implementation-plan/evidence/T04.md) |

## Nutzerentscheidungen dieser Arbeit

Die Details stehen in den Nachweisen und in `packets.json` (Feld `boundary`).

- **L05 (07.10.2026):**
  - Dialog beim Start.
  - Sicherung 30 s nach der letzten Änderung, spätestens alle 2 min.
  - Ein offener Skizzenentwurf wird getrennt gesichert und im Editor wiederhergestellt.
- **T04 (07./08.10.2026):**
  - Anlass: Der Nutzer hat SketchUp und den Python-Nachbau IngeTrazo verglichen.
    Ergebnis: Völund bleibt parametrisch und übernimmt das schnelle Drücken/Ziehen.
  - „Fläche als Profil“: ein Feature.
  - „Pfeil ziehen + Maßfeld“, als nächstes Paket.
  - Der Pfeil ist größer und zeigt beim Drücken ins Bauteil.
  - Die Features heißen „Ziehen N“ / „Drücken N“.
  - Aufnahmen freigegeben („passt soweit“).
- **T05 (08.10.2026):** Koplanare Flächen verschmelzen „später als eigenes Paket“. Neu in
  `packets.json`, Zuschnitt in [T-plan.md](implementation-plan/T-plan.md).

## Nächste freie Pakete (nach Abhängigkeiten)

- **S03** Skizze aus leerem Dokument auf einer Basisebene (G2).
- **S08** Transformationen und Mehrfachauswahl (G2).
- **T05** Koplanare Flächen nach Drücken/Ziehen verschmelzen (G3). Laut Nutzer „später“;
  nicht vor S03/S08 ohne Rückfrage.
- **A02** braucht weiterhin Designarbeit.

Der Nutzer hat bestätigt: Es bleibt bei der Reihenfolge nach Plan. M12 (Messen) und M17
(STEP) werden nicht vorgezogen.

## Offene Punkte

- **CI-Runner umziehen** (Nutzer: „morgen entscheiden – don't touch a running system“):
  - Der Runner läuft heute als Host-Executor in der KDE-Sitzung des Fedora-Laptops.
  - Vorschlag: eine eigene Fedora-44-VM in Proxmox, gegebenenfalls als zweiter Runner
    mit Label `linux-desktop`.
  - Der vorhandene Proxmox-Runner ist ein Docker-Runner mit Ubuntu-Labels und taugt dafür
    nicht direkt (Qt 6.11, OCCT 7.9.3 nötig).
  - **Am Runner nichts ändern, bevor der Nutzer entscheidet.**
- **CI-Lauf 453** (L05a) war rot: Alle Fenstertests liefen in die Zeitgrenze. Die Ursache
  ist offen; ein paralleler Lauf scheidet aus (Kapazität 1). Seitdem ist alles grün.
- **Grenzen aus T04:**
  - Koplanare Flächen bleiben getrennt (sichtbare Naht, wird T05).
  - Die Kanten einer gezogenen Deckfläche haben keine Namen.
- **Unverändert offen:**
  - Spiegelzellen per RegionId (Nutzerentscheidung nötig).
  - PAR02 (Parameter-UI).
  - Der GitHub-Spiegel ist nicht aktuell; nur auf Wunsch synchronisieren.

## Ergänzungen zu Werkzeugen und Fallstricken

- **Testfilter unter Windows:** `ctest -R "a|b"` über `cmd` zerlegt das `|`. Tests einzeln
  aufrufen.
- **`\n` in C++-Strings:** Über ein Bash-Heredoc mit eingebettetem Python wird es zum
  echten Zeilenumbruch. Längere Patches als Datei per Write-Werkzeug ablegen oder mit dem
  Edit-Werkzeug ändern.
- **Recovery in Tests:** `IsolatedSettings.hpp` setzt `VOELUND_RECOVERY_DIR` auf ein
  temporäres Verzeichnis. Fenstertests schreiben so nie in das echte
  Wiederherstellungsverzeichnis.
- **Flächenrahmen:** Herkunftslisten speichern Flächen ohne ihre Orientierung im Körper.
  Die Außennormale kommt aus dem Vorkommen im Volumenkörper (`supportFaceFrames`,
  `FaceExtrusionEvaluation`).
- **Aufnahmen für den Nutzer:** Er sieht keine Bilder aus dem Chatverlauf. Die Dateien als
  Link aus dem Scratchpad nennen.
