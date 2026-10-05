# Fortsetzung – 4. Oktober 2026

**Einstieg für die nächste Sitzung:** Dieses Dokument zuerst lesen. AI02 ist
abgeschlossen. Seit 05.10.2026 kommt **AI04** (MCP-Zugang für Claude Code) vor AI03, siehe
[AI04-plan.md](implementation-plan/AI04-plan.md).

## Stand des Repos

- Arbeitsbranch `ux01`; `main` wird nach grüner CI per Fast-Forward nachgezogen (siehe
  Arbeitsregeln). Der letzte Abnahme-Commit (AI02) folgt nach seinem CI-Lauf auf `main`;
  vorher stand `main` auf `e768d03`.
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
| AI02a | Typisierte Requests, Kontext, Prüfung vor jeder Transaktion | 375 | [AI02a](implementation-plan/evidence/AI02a.md) |
| AI02b | Explizite Ziele für Box, Zylinder, Platzierung | 378 | [AI02b](implementation-plan/evidence/AI02b.md) |
| AI02c | Vorschau, Skizze + Extrusion als ein Commit, Request-IDs | 380 | [AI02c](implementation-plan/evidence/AI02c.md) |
| AI02d | Shell-Befehle laufen als Requests | 382 | [AI02d](implementation-plan/evidence/AI02d.md) |

Außerdem:

- `fdaf24c` macht den Skizzenlöser deterministisch. PlaneGCS ordnet Unbekannte nach
  Adressen; `desktop.sketch_snap` scheiterte vorher in etwa 40 % der Läufe.
- Seit U12a gibt es nur noch eine Oberfläche, die Shell nach ADR 0012.

## Nächste Schritte (Reihenfolge)

1. **AI04a–d** laut [AI04-plan.md](implementation-plan/AI04-plan.md): lokaler MCP-Server
   in Völund für Claude Code (Nutzerentscheidung 05.10.2026: kein API-Schlüssel in Völund,
   Änderungen direkt mit Undo, erster Fall mit vorhandenen Features). Danach **AI03**
   (Seitenpanel, das Claude Code mit diesem Server startet).
   - Grundlage ist `include/voelund/application/Actions.hpp`: `describeActionContext`,
     `ActionDispatcher` (Wiederholungsschutz) und `preview` für Vorschläge vor dem Commit.
   - Absehbare Lücken: Skizzen auf Körperflächen ohne GUI (Flächenbezug über
     `SubshapeRef`), allgemeine mehrteilige Stapel mit Verweisen auf frühere Schritte,
     Bohrung und Rippe als Features gibt es noch nicht.
2. Danach U07–U10 und die weiteren Pakete laut `topologicalOrder` in
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
    `ctest --test-dir build/p02-qt-final -j1`. Stand heute: 113 Tests.
  - Git braucht `-c safe.directory='*'`.
  - Desktop-Tests laufen unter Windows auf dem echten Desktop. Mausbewegungen des Nutzers
    können Hover-Tests stören (`sketch_snap`).
- **Fedora-Nachbau** für Experimente unter Linux:
  - `~/repro/par01` ist ein Clone mit dem Lese-Token. Aktualisieren mit
    `git -c http.extraHeader="Authorization: token $T" fetch`.
  - Headless-Sitzung wie in der CI: `kwin_wayland --virtual` (siehe `~/repro-snap.sh`).
  - Windows nutzt OCCT 8, Fedora OCCT 7.9.3. Beispiel: `Standard_Failure::what()` gibt es
    nur in 8, `GetMessageString()` ist dort veraltet. Versionsneutral bleiben.
- **GCC vor dem Push:** MSVC lässt Dinge durch, an denen die CI scheitert, z. B.
  `-Werror=missing-field-initializers` bei unvollständigen Aggregaten (Lauf 374). Vorab
  auf Fedora bauen: in `~/repro/par01` `fetch` und `checkout -f <HEAD>`, dann die
  geänderten Dateien per `tar cf - … | ssh … 'tar xmf -'` übertragen (`m` setzt neue
  Zeitstempel, sonst baut ninja nicht neu) und `cmake --build --preset linux-desktop`.
- **Kodierung:** Python unter Windows schreibt ohne Angabe cp1252. Immer
  `encoding='utf-8'` angeben.
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
