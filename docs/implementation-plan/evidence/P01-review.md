# P01 – Gegenprüfung vom 13.09.2026

**Historischer Reviewbefund; durch die Korrekturen und Abnahme in [P01.md](P01.md) geschlossen.**
Die folgenden Befunde und Proben dokumentieren den Stand vor der Korrektur.
Die vorhandenen Windows-Läufe werden dadurch nicht als fehlgeschlagen umgedeutet.
Die Berichts- und Abnahmelogik erfüllt den Paketvertrag jedoch noch nicht.

## Reproduzierte Abnahmefehler

Die vorhandenen `PlatformReport.Tests.ps1` bestehen mit Exit 0. Zusätzliche
Proben auf Basis des vorhandenen Qt-OpenGL-Berichts ergeben:

| Probe | Tatsächlich | Erforderlich |
|---|---|---|
| `matrix=[]`, `results=[]`, `source=null`, `configuration=null`, `capture=null`, mit `-RequirePlatformAcceptance` | Exit 0, `platformAcceptance=passed` | Exit 1, ungültiger Bericht |
| `passed`-Ergebnis mit einzigem Artefakt `definitely-does-not-exist.log` | Exit 0, `structuralValidity=passed` | Exit 1, fehlende Evidenz |
| Alle Matrixzeilen `passed`, aber Ergebnis `failed` mit Exitcode 1, mit `-RequirePlatformAcceptance` | Exit 0, `platformAcceptance=passed` | Kein bestandener Gesamtnachweis; widersprüchliche Zuordnung zurückweisen |

Die Proben und Konsolenausgaben liegen unter
[`P01-review-artifacts`](P01-review-artifacts/review.log). Es wurden keine neuen
Produktbuilds oder GUI-Läufe für diese Gegenprüfung ausgeführt: Die Fehler liegen
in der Berichtslogik und sind ohne erneuten Produktstart reproduzierbar.

## Korrekturauftrag, ausschließlich P01

### P01a – Schema und Abnahme konsistent validieren

1. Pflichtobjekte auf Typ und Inhalt prüfen, nicht nur auf vorhandene Schlüssel.
   `if ($report.matrix)` darf leere Pflichtdaten nicht aus der Prüfung ausnehmen.
   Matrix muss exakt fünf Einträge mit exakt den fünf eindeutigen Ziel-IDs haben;
   doppelte Einträge ebenfalls zurückweisen. Ergebnisse dürfen nicht leer sein.
2. Inhaltliche Pflichtwerte, UTC-Zeit, Revision, Hashformat, Compilerdaten und
   begründete unbekannte/nicht anwendbare Werte validieren. Leere Maps und beliebige
   Strings sind kein vollständiger Quellstand- oder Umgebungsnachweis.
3. Matrixzeilen eindeutig mit Ergebnissen/Nachweisen der jeweiligen Konfiguration
   verknüpfen. Eine Zeile darf keinen Erfolg behaupten, den die zugeordneten
   Prüfungen nicht belegen. Ein Windows-Lauf darf keine FreeBSD-Abnahme erzeugen.
4. Artefakte wirklich auf Existenz und passenden Typ prüfen, mit dokumentierter
   Pfadauflösung. JUnit-Inhalt prüfen: positive Testzahl, keine unerwarteten
   Fehler und die erforderlichen Fälle. Ein vorhandener leerer oder fremder
   Logpfad genügt nicht. Erwartete Negativsmokes sind bestandene CTest-Fälle.
5. Strukturelle Gültigkeit und Plattformabnahme weiterhin trennen. Gültige
   blocked/skipped-Zeilen bleiben zulässig, dürfen die Abnahme aber nicht bestehen.
6. Die drei Proben oben sowie null/leere Pflichtfelder, doppelte Matrix-ID,
   fehlende Datei, leerer JUnit, Backendwiderspruch und gültige Blocker als
   dauerhafte Regressionen aufnehmen. Diese Berichtsregression in den bestehenden
   Windows-CTest-Weg integrieren, damit der vollständige Lauf sie künftig erfasst.

### P01b – Aktuelle Artefakte und tatsächliche Laufzeitidentität

`New-PlatformReport.ps1` liest gegenwärtig die erste beliebige JSON-Datei im
Artefaktverzeichnis. Die Aufnahmeumgebung wird nur außerhalb des Skripts gesetzt;
vorhandene alte PNGs können als Aufnahmen des neuen Laufs gelten. Backend wird
aus dem Cache übernommen und QPA auf `windows` festgesetzt, ohne Abgleich mit
den eingelesenen Laufzeitdaten. Das ist eine Lücke im Erzeuger, kein Nachweis,
dass die vorliegenden Bilder tatsächlich falsch sind.

1. Ein eigenes, frisches Unterverzeichnis je Lauf anlegen. Aufnahmeziel für den
   Testprozess ausdrücklich setzen und die vorherige Umgebungsvariable im
   `finally`-Pfad wiederherstellen. Bestehende Evidenz nicht überschreiben.
2. Ausschließlich die erwarteten CAD-/Preview-Diagnosen und Aufnahmen dieses
   Laufs einlesen. Beide Anwendungen abgleichen: Backend, QPA, Versionen und
   benötigte Renderdaten. Widersprüche als Fehler erfassen, nicht überdecken.
3. `-SkipTests` darf alte Bilder/Diagnosen nicht als neuen Lauf ausgeben.
   Visuelle Abnahme an konkret betrachtete aktuelle Bilder binden; keine
   pauschale Übernahme alter `visually-reviewed`-Markierungen.
4. Fremde JSON-Dateien, alte Aufnahmen, Backendwiderspruch und Wiederholung mit
   schon vorhandenem Zielverzeichnis gezielt prüfen. Fehler/Timeouts sollen einen
   ehrlichen Bericht hinterlassen; ein fehlgeschlagener Testlauf darf vom
   Erzeuger nicht als erfolgreicher Ausführungsprozess signalisiert werden.

### P01c – Quellstand und Wiederholung nachvollziehbar machen

Die aktuellen sechs Dateihashes decken unter anderem Kernel, Viewport und
Anwendungseinstiege nicht ab. Bei dem umfangreichen uncommitteten Arbeitsstand
identifizieren Commit plus Dirty-Flag damit nicht den gebauten Produktstand.
Außerdem führt der Erzeuger keinen Build aus und erfasst die Hashes erst nach
den Tests. Paketstatus wird fest kodiert statt aus dem aktuellen Plan gelesen.

1. Einen deterministischen Quellstandnachweis für den buildrelevanten eigenen
   Code einschließlich untracked Quellen erzeugen; generierte Ausgaben und
   Evidenz passend ausschließen und den Scope dokumentieren. Vor und nach
   Build/Test vergleichen, Änderungen während des Laufs zurückweisen.
2. Aktuellen Build im geprüften Setup-Kontext durchführen oder einen überprüfbaren
   passenden Buildnachweis voraussetzen. Ausführbare Test-/Produktdateien und
   relevante Konfiguration dem Lauf zuordnen. Keine aktuellen Quellhashes an
   beliebige ältere Binärdateien heften.
3. Paketstatus aus `packets.json` übernehmen. Unbekannte Werte einschließlich
   Display-/Skalierungsfeldern einzeln begründen; nicht blind mit null ohne
   Begründung füllen. Presetwahl von tatsächlich gemessenen Laufzeitdaten trennen.
4. Beide Windows-Berichte nach den Korrekturen in neuen Verzeichnissen erzeugen,
   validieren, die relevanten aktuellen Bilder selbst betrachten und die
   Wiederholungsbefehle einschließlich Build vollständig dokumentieren.
5. Evidenz/Manifest erst danach zur erneuten Abnahme aktualisieren. Alte Berichte
   als historischen Stand erhalten. FreeBSD/Linux-Blocker bleiben ehrlich offen;
   G0 und P04 werden durch diesen Auftrag nicht geschlossen.

Keine P02-Arbeiten, kein Python, kein Dependencyupgrade, keine neuen CAD-Funktionen.
Vorhandene fremde Änderungen erhalten. Die Korrekturen und Tests benötigen keine
manuelle Nutzerprüfung. Nach P01 zur Gegenprüfung berichten, nicht weitergehen.
