# AI02c – Kandidat, Vorschau, atomarer Commit, Wiederholung

**Status:** umgesetzt, CI-Lauf ausstehend. Teilplan: [AI02-plan.md](../AI02-plan.md).

## Umsetzung

- **Vorschau (`previewAction`, `ActionDispatcher::preview`):**
  - Der Request läuft auf einer Kopie der `CadSession`. Die Kopie ist billig: Die
    OCAF-Zustände sind unveränderlich und werden erst beim Commit ersetzt, die Auswertung
    wird per `shared_ptr` geteilt. Commits der Kopie erreichen die Session nie.
  - `ActionPreview` liefert das vollständige `ActionResult` (Status, Diagnosen, erzeugte
    und geänderte IDs) und die gültigen Körper des Kandidaten. `result.stamp` ist der
    unveränderte Stand der Session.
  - Der Commit führt denselben Request später erneut aus und prüft dabei den Stand. Eine
    Nutzeränderung dazwischen führt zu `staleStamp`.
  - **Abweichung vom Teilplan:** `finishCandidate` wird nicht geteilt. Die Kopie
    erreicht dasselbe ohne Eingriff in die vorhandenen Befehle, und der Commit wertet
    nachweislich den aktuellen Stand aus statt eines gemerkten Kandidaten.
- **Mehrteilige Aktion `CreateSketchExtrusion`** und
  `CadSession::createDatumSketchExtrusion`:
  - Basisebenen-Skizze und Extrusion auf **einer** `DocumentTransaction`, ein Commit, ein
    Undo-Schritt.
  - Die Regionen werden aus den Elementen berechnet, die der Kandidat gespeichert hat.
    Eine leere Auswahl nimmt alle eindeutigen Regionen.
  - Scheitert Schritt 2 (Distanz, keine geschlossene Region, unbekannte Region), verwirft
    die Transaktion auch Schritt 1.
  - Die Aktionsschicht löst angegebene Regionen schon vor der Transaktion gegen die
    übergebenen Elemente auf (`unresolvedReference`).
- **Wiederholungsschutz (`ActionDispatcher::execute`):**
  - Tabelle der letzten 256 Requests der aktuellen Dokumentgeneration (Request und
    Ergebnis). Sie wird vor der Standprüfung befragt, weil eine Wiederholung den Stand
    trägt, den ihre erste Ausführung verbraucht hat.
  - Gleicher Inhalt liefert das gespeicherte Ergebnis mit `repeated = true`, ohne neue
    Mutation. Anderer Inhalt unter derselben ID (auch ein anderer Stand) ergibt
    `requestConflict`. Ein Request ohne ID wird abgewiesen (`missingRequestId`).
  - Statt des im Teilplan genannten Grundes `duplicateRequest` meldet das Ergebnis die
    Wiederholung über `repeated`: Die Wiederholung ist keine Ablehnung, sondern die
    Antwort auf die erste Ausführung.
  - Inhaltsgleichheit über `operator==` aller Aktionen, kein Hash. Die Tabelle gilt nur
    im Speicher und nicht über Speichern und Öffnen hinweg.

## Prüfungen

| Test | Inhalt |
|---|---|
| `application.action_candidates` | Skizze + Extrusion in einem Request: zwei neue Features, Volumen 500π; genau ein Undo entfernt beides, Redo stellt den identischen Stand her. Fehler in Schritt 2 (negative Distanz, offene Skizze) und unbekannte Region: Dokument, Stand, Undo und Redo bleiben gleich. Vorschau: Ergebnis und Körper des Kandidaten, aktive Session unverändert; nach einer Nutzeränderung scheitert der Commit des vorgeschauten Requests mit `staleStamp`, ein neuer Request committet. Wiederholung derselben ID liefert das gespeicherte Ergebnis ohne Mutation; anderer Wert oder anderer Stand unter derselben ID: `requestConflict`; leere ID abgewiesen. Nach 256 neueren Requests ist die erste ID vergessen; nach Neu ist eine ID wieder frei |

- **Windows:** 112/112 Tests bestanden (Debug, MSVC, seriell).
- **Linux:** CI-Lauf ausstehend. Vorab auf Fedora mit GCC gebaut: 31/31 Tests der
  Anwendungsschicht.

## Grenzen

- Mehrteilig ist zunächst nur Skizze + Extrusion. Allgemeine Stapel mit Verweisen auf IDs
  früherer Schritte kommen erst, wenn AI03 sie braucht.
- Ein Request mit NaN-Werten ist sich selbst nicht gleich; seine Wiederholung ergibt
  `requestConflict` statt `repeated`. Er wurde ohnehin als `nonFinite` abgewiesen.
- Die Vorschau wertet einmal und der Commit ein zweites Mal aus.
