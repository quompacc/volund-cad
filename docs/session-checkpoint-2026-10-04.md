# Fortsetzung – 4. Oktober 2026

**Einstieg für die nächste Sitzung:** Dieses Dokument zuerst lesen. Danach
[AI02-plan.md](implementation-plan/AI02-plan.md) lesen und mit **AI02a** beginnen.

## Stand des Repos

- Arbeitsbranch `ux01`; `main` wird nach grüner CI per Fast-Forward nachgezogen (siehe
  Arbeitsregeln). Beim Schreiben dieser Notiz war `main` = `1a8eb7d`. Der Abnahme-Commit
  `5182597` (R10b, AI-Umstellung) wartete noch auf seinen CI-Lauf und wird danach nach
  `main` gebracht.
- Remotes: `origin` = Gitea `ssh://git@192.168.1.11:2222/eduard/volund-cad.git`
  (maßgeblich, mit CI), `github` = `git@github.com:quompacc/volund-cad.git` (wird nicht
  automatisch gepflegt).
- Keine weiteren Worktrees oder Branches. Der frühere Branch `par01` ist gelöscht, sein
  Inhalt steckt in `ux01`.

## Heute abgenommen

| Paket | Inhalt | CI-Lauf | Nachweis |
|---|---|---|---|
| U13 | Werkzeugleiste folgt dem Skizzenmodus | 358 | [U13](implementation-plan/evidence/U13.md) |
| V01 | Durchstich in der neuen Oberfläche | 358 | [V01c](implementation-plan/evidence/V01c.md) |
| S04a | Fangmarker, Rechteck aus der Mitte | 358 | [S04a](implementation-plan/evidence/S04a.md) |
| PAR01 | Dokumentparameter, Bindungen, Schema 7 (ohne UI) | 363 | [PAR01](implementation-plan/evidence/PAR01.md) |
| U12a | UI-Vorschau und toter Rechteckweg entfernt, Test-Einstellungen isoliert | 366 | [U12a](implementation-plan/evidence/U12a.md) |
| R10b | Faces für Regionen schneidender Konturen | 370 | [R10b](implementation-plan/evidence/R10b.md) |

Außerdem:

- `fdaf24c` macht den Skizzenlöser deterministisch. PlaneGCS ordnet Unbekannte nach
  Adressen; `desktop.sketch_snap` scheiterte vorher in etwa 40 % der Läufe.
- Seit U12a gibt es nur noch eine Oberfläche, die Shell nach ADR 0012.

## Nächste Schritte (Reihenfolge)

1. **AI02a–d** laut [AI02-plan.md](implementation-plan/AI02-plan.md): eine gemeinsame,
   zielgerichtete Aktionsschicht für GUI, Assistent und MCP. Ohne Netzwerk und ohne SDK.
2. **AI03** integrierter Assistent: Claude per API, Adapter austauschbar, ChatGPT möglich.
   Vor Beginn mit dem Nutzer abstimmen: Bedienung im UI (Seitenpanel nach ADR 0012),
   Ablage des API-Schlüssels, erster Anwendungsfall laut Kapitel 11 (Referenzplatte,
   Bohrung, Rippe).
3. Danach U07–U10 und die weiteren Pakete laut `topologicalOrder` in
   [packets.json](implementation-plan/packets.json).

Hintergrund ist die Nutzerentscheidung vom 04.10.2026 ([Kapitel 11](implementation-plan/11-ki-fem-optimierung.md),
[README](implementation-plan/README.md)). Die Regel „AI02 vor U04“ war mit UX01
übersprungen worden. AI02 schließt diese Lücke, bevor weitere Befehle entstehen.

## Arbeitsregeln (verbindlich)

- **Ein Paket, ein Ablauf:**
  1. Umsetzen.
  2. Lokal unter Windows bauen und alle Tests ausführen.
  3. Commit auf `ux01` und Push. Die Linux-CI startet automatisch.
  4. Bei grünem Lauf den Abnahme-Commit erstellen: `packets.json` mit `status: done`,
     `evidence` und `completedCommit`, die Lauf-ID im Nachweis, die Zähler in
     `implementation-plan/README.md`.
  5. Wenn auch dessen Lauf grün ist: `git push origin <commit>:main`, nur als
     Fast-Forward, nie mit Force. Der Branch-Schutz auf `main` verlangt den Status
     `ci / linux-desktop*`.
- **Nachweise:** kurze Notiz `docs/implementation-plan/evidence/<ID>.md` mit Umsetzung,
  Prüfungen (CI-Lauf-ID, Windows-Ergebnis) und Grenzen. Keine Logs, keine Quellhashes.
  Aufnahmen nur bei sichtbaren Änderungen.
- **Oberfläche:** ADR 0012 ist das freigegebene Zielbild. Sichtbare Abweichungen vorher mit
  dem Nutzer abstimmen und Aufnahmen vorlegen.
- **Dateigröße:** höchstens 600 Zeilen je Quelldatei (`architecture.source_limits`).
- **Parallelität:** nie zwei Sitzungen im selben Worktree. Vor Beginn mit `git status`
  und `git worktree list` prüfen.

## Werkzeuge und Zugänge

- **CI-Stand prüfen.** Das Repo ist privat; die API von Windows aus liefert ohne Token 404.
  Der Weg führt über den Fedora-Testrechner, wo der Lese-Token des Nutzers liegt:

  ```sh
  ssh -i ~/.ssh/meridian_freebsd -o IdentitiesOnly=yes -o BatchMode=yes eduard@192.168.1.203 \
    'curl -fsS -H "Authorization: token $(cat ~/.config/volund/gitea-read-token)" \
     "https://gitea.hl.home.arpa/api/v1/repos/eduard/volund-cad/actions/runs?limit=5"'
  ```

  - `id` ist die Lauf-ID für Nachweise, `run_number` das „ci #N“ der Oberfläche.
  - Job-Logs: `…/actions/runs/<id>/jobs`, dann `…/actions/jobs/<job>/logs`.
  - Artefakte (`ctest.junit.xml` mit Testausgabe): `…/actions/runs/<id>/artifacts`, dann
    `…/actions/artifacts/<aid>/zip`.
  - Remote-Befehle mit dem Bash-Werkzeug ausführen. PowerShell verändert `$` und
    `/dev/null`.
- **Windows-Build:**
  - `subst V: "D:\300_Projekte\310_Aktiv\volund-cad"`, dann
    `"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"`
    (`vswhere` fehlt) und von `V:\` aus `cmake --build build/p02-qt-final` sowie
    `ctest --test-dir build/p02-qt-final -j1`. Stand heute: 109 Tests.
  - Git braucht `-c safe.directory='*'`.
  - Desktop-Tests laufen unter Windows auf dem echten Desktop. Mausbewegungen des Nutzers
    können Hover-Tests stören (`sketch_snap`).
- **Fedora-Nachbau** für Experimente unter Linux:
  - `~/repro/par01` ist ein Clone mit dem Lese-Token. Aktualisieren mit
    `git -c http.extraHeader="Authorization: token $T" fetch`.
  - Headless-Sitzung wie in der CI: `kwin_wayland --virtual` (siehe `~/repro-snap.sh`).
  - Windows nutzt OCCT 8, Fedora OCCT 7.9.3. Beispiel: `Standard_Failure::what()` gibt es
    nur in 8, `GetMessageString()` ist dort veraltet. Versionsneutral bleiben.
- **Zeilenenden:** Windows checkt mit CRLF aus. Python-Skripte, die Dateien bearbeiten,
  müssen die Zeilenenden beibehalten (`newline=''`), sonst greifen Ersetzungen nicht.

## Offene Punkte und Nutzerentscheidungen

- **Spiegelzellen per RegionId**, z. B. die Hälften einer Sehne: Die Faces entstehen
  (R10b), die Zellen sind per ID aber bewusst mehrdeutig (R09). Eine Unterscheidung über
  die Linienrichtung wurde verworfen, weil ein umklappender Löser eine alte ID still auf die
  andere Hälfte lenken würde. Das braucht eine Nutzerentscheidung.
- **PAR02** (Parameter-UI) steht noch aus. Bis dahin weicht ein im Skizzeneditor direkt
  geändertes gebundenes Maß vom Parameter ab.
- **Aus der UI-Bestandsaufnahme offen:** modaler `QInputDialog` für Skizzenmaße,
  Reparaturdialoge (U10), Composition Root und Installation (U12).
- **GitHub-Spiegel** (`github`) ist nicht auf dem aktuellen Stand. Synchronisieren nur auf
  Wunsch.
