# 08 – Verbindlicher Abnahmekatalog

Diese Fälle sind **geplante Abnahmen**, nicht bereits bestandene Tests. Kapitel01 nennt ausschließlich die tatsächlich ausgeführten Prüfungen. Die Paketumsetzung legt passende native Testdateien und registrierte CTest-Namen an. IDs in diesem Katalog bleiben stabil, auch wenn Testdateien nach Verantwortlichkeit aufgeteilt werden.

## Allgemeine Prüfregeln

- Fachwerte unabhängig vom Implementierungsweg herleiten: analytische Volumina/Flächen, bekannte Positionen und explizite IDs. Kein Vergleich eines Algorithmus mit derselben Funktion als Oracle.
- Floatvergleich: `abs(actual - expected) <= absTol + relTol * abs(expected)`. D10 legt dokumentierte dimensionale Werte je Länge/Fläche/Volumen fest; nicht überall denselben dimensionslosen Epsilonwert verwenden. Werte nicht nachträglich aufweiten, nur um einen Fehler grün zu machen.
- Geometrie prüft Shape-Validität, Topologieart, Körperanzahl, Lage und Maß/Volumen; „nicht null“ reicht nicht.
- Transaktion prüft Parameter, IDs, Revision, Dirty, History, Selection und relevante Fehlerzustände. Dateifehler prüfen zusätzlich erhaltene Bytes.
- UI-Fälle laufen durch echte Events/Registry/Controller nach Eventloopstart. Ein Screenshot zeigt Darstellung, beweist aber keine Dokumentmutation.
- Der Agent führt Tests und optische Kontrolle selbst aus und behebt Abweichungen gemäß [Kapitel 10](10-autonome-umsetzung.md). Die Erwähnung manueller oder realer Plattformfälle bedeutet nicht automatisch eine Aufgabe für den Nutzer. Nur ein belegter, technisch nicht selbst prüfbarer Rest wird gebündelt vorgelegt.
- Jeder Fehlerfix bekommt den kleinsten reproduzierenden Fall plus gegebenenfalls das betroffene Referenzmodell. Für Dependency-Referenzdaten gelten Herkunft und zulässige Weitergabe; kein FreeCAD/Python im normalen Testlauf.

## A-SAVE-01 – Fehlertolerantes Ersetzen einer vorhandenen Datei

**Start:** Dokument A mit bekanntem Snapshot und bestehender Zieldatei. Datei-Bytes und Save-Revision merken. Parameter zu Zustand B ändern.

**Aktion:** Beim Schreiben nacheinander Fehler vor Temp-Erstellung, während Schreiben, Flush, Close und Replace injizieren. Zusätzlich echtes read-only/gesperrtes Ziel prüfen, soweit die Plattform dies unterstützt.

**Erwartung:** Für jeden Fehler vor erfolgreicher Ersetzung bleibt die alte Datei bytegenau erhalten; Dokument B bleibt dirty, ursprünglicher Pfad unverändert, Fehlerrückgabe benennt Phase. Kein fremdes Tempfile löschen. Erfolgsfall lädt exakt B und setzt Save-Revision erst nach Abschluss.

**Ebene/Pakete:** Plattform + Application; D01/D03/D13.

## A-SAVE-02 – Save-As und reparierbare Fehlerzustände

Save-As zu unbeschreibbarem Ziel darf weder Titel/Pfad noch Save-Revision ändern. Ein bewusst übernommenes Modell mit kaputter nachgelagerter Referenz lässt sich speichern und wieder öffnen; Fehlerstatus und editierbare Parameter bleiben erhalten. Die Datei wird nicht abgelehnt, nur weil kein vollständig gültiger Endkörper existiert. Ein unbekanntes Dateischema bleibt dagegen ein Ladefehler.

## A-IO-01 – Laden ohne Verlust des aktiven Dokuments

Aktives dirty Dokument A, beschädigte/abgeschnittene/zu große/falsche Version B öffnen. Bei Cancel oder Ladefehler bleibt A vollständig aktiv. Nur erfolgreicher Import mit bestätigtem Umgang mit A ersetzt das Dokument. Pfade enthalten `Prüfung mit Leerzeichen/Teil äöü.volund`; auf Windows und FreeBSD prüfen. Auswahl/Kamera können nach erfolgreichem Open neu initialisiert werden, aber keine Parametermutation auslösen.

## A-MIG-01 – Jede historische Formatversion

Für VOLUND1–16 mindestens je eine minimale gültige Fixture und ein passender ungültiger Fall. Historische Merkmale abdecken: Rechteck/Lines, Richtung, Constraints/Extrude, Profilwahl, PointLock, Equal/Tangent, Symmetry, Distance/Angle, Construction/dx/dy, Midpoint/PerpendicularBisector, Concentric/Diameter, Collinear/Alignment, Supportreferenzen, Arc, Ellipse. Tatsächliche Versionszuordnung aus dem bestehenden Reader übernehmen.

Je Fixture: lesen → erwartete fachliche Werte prüfen → neues Format schreiben → öffnen → Parameter ändern → Undo/Redo. IDs/Beziehungen erhalten; Defaultwerte explizit dokumentiert. Legacy-Referenz bei Mehrdeutigkeit benötigt Reparatur und wird nicht geraten. Quelldatei unverändert lassen. Unbekannte Zukunftsversion mit deutlicher Meldung abweisen.

## A-TXN-01 – Zusammengesetzter Befehl

Startzustand A enthält genau einen Grundkörper. Ein einzelner `addSketchExtrusion`-Befehl erzeugt intern Sketch und Extrude. Ein Undo stellt A einschließlich IDs/Dirty/History wieder her; ein Redo erzeugt denselben fachlichen Zustand B. Separat bestätigte Werkzeuge Sketch-OK und Extrude-OK haben dagegen jeweils einen eigenen Schritt.

## A-TXN-02 – Rollback bei Teilfehler

Nach erster interner Mutation eines zusammengesetzten Befehls gezielt Fehler auslösen. Kein Teilfeature bleibt zurück, nächste erfolgreiche ID kollidiert nicht, Redo-Stack folgt festgelegter Transaktionssemantik. Save→Edit→Undo zur gespeicherten Revision ist clean. Undo/Redo zeichnet sich nicht selbst erneut auf.

## A-TXN-03 – Drag und Refresh

Skizzenelement auswählen, Drag beginnen, 100 Updates senden, beenden: genau ein Undo. Zweiten Drag per Escape abbrechen: keine bleibende Änderung und kein neuer Eintrag. 100 Refresh/Redraw-Aufrufe: null History-/Dirty-Änderungen. Dasselbe für Körpermanipulator prüfen. Selection und Constraint-/Solve-Status stimmen nach Rückkehr überein.

## A-ERR-01 – Fehler statt falschem Erfolg

Solverfehler, konflikthafte Skizze und Kernel-Operationsfehler getrennt auslösen. Neuer ungültiger Kandidat wird rejected; Editor bleibt korrigierbar. Bei bestehendem Feature kann ausdrücklich mit Folgefehlern committed werden. Anzeige/Tree/Timeline zeigen Failed/Blocked/stale richtig; kein gültiger Export des fehlerhaften aktuellen Resultats. Repair und Undo der Repair führen deterministisch zwischen den Zuständen.

### Nachweisstufen und zuständige Pakete

Die vollständige A-ERR-01-Produktabnahme bleibt verpflichtend. D07 kann nicht zugleich
die erst später eingeführte Timeline und STEP-Oberfläche abnehmen; U10 setzt D07
sogar ausdrücklich voraus. Deshalb werden die Nachweise in dieser Reihenfolge geführt:

| Stufe | Verbindlicher Nachweis | Zuständig |
| --- | --- | --- |
| Fundament | Solver-Failed/Conflicting getrennt; typisierte Ablehnung und explizite Fehlerübernahme; Exception-Grenzen; Editor bleibt korrigierbar; aktueller Geometriezugriff bei stale gesperrt; Fehler/Blocked im bestehenden Tree; Repair/Undo/Save/Open | D07 |
| Abhängigkeiten und gemeinsame Shell | Nur tatsächliche Nachfolger blockieren; unabhängige Zweige bleiben gültig; Reparatur/Referenzdiagnosen und Messsperren in der zusammengeführten Oberfläche | R01, U10 |
| Timeline | Fehler/Blocked/Suppressed und History-Cursor aus derselben aktuellen Projektion anzeigen | M15 |
| Austausch und Gesamtprodukt | Fehlerhafte aktuelle Revision nicht exportieren; produktiven Export-/Fehlerpfad ausführen; vorstehende Stufen gemeinsam im Produkt verifizieren | M17, M18 |

Eine bestandene Fundamentstufe ist keine vollständige A-ERR-01-Produktabnahme.
Die spätere Timeline-/Exportpflicht wird weder durch eine API-Sperre noch durch
eine fehlende oder deaktivierte Schaltfläche als bestanden gewertet.

## A-NUM-01 – Einheiten, Ebenen und Grenzwerte

Modelle mit charakteristischer Länge 0,01 mm, 100 mm und 100.000 mm verwenden; D10 dokumentiert, welche Größen zum unterstützten Bereich gehören. NaN/Inf/Nullachse/negative Maße und ID-Überlauf vor Mutation abweisen. Rechtshändige Ebene mit nicht normierten Achsen gemäß festgelegtem Vertrag normalisieren oder ablehnen. Rotation/Save/Open verändert Skala nicht. Knapp unter/über Verbindungstoleranz liegende Endpunkte deterministisch klassifizieren.

## A-DAG-01 – Unabhängige und verzweigte Abhängigkeiten

Zwei Körper A/B mit jeweils Skizze und Extrude. Fehler nur im A-Zweig erzeugen: B bleibt gültig; nur A-Nachfolger blocked. Ergänzend Diamond-DAG mit gemeinsamem Input und zwei Nachfolgern sowie einem Merge testen. Zyklen werden vor Commit abgewiesen. Unabhängiges Reorder erhält BodyId und aktuelle richtige Outputs.

## A-REF-01 – Ähnliche Flächen nicht vertauschen

Zwei parallele bzw. koplanare geometrisch ähnliche Flächen als Kandidaten herstellen. Skizze explizit auf Fläche F anbinden. Andere Fläche erzeugen, Reihenfolge/Tessellierung ändern und Grundkörpermaß verändern. Bezug bleibt F bzw. dessen eindeutig historisch abgeleitetem Nachfolger. Nähe und Normalengleichheit allein dürfen keinen Wechsel verursachen.

## A-REF-02 – Translation, Rotation, Split und Löschung

Stützfläche samt Körper verschieben und um90° rotieren: Skizzenbasis folgt korrekt. Referenzierte Fläche per Operation splitten bzw. löschen: vereinbarte eindeutige Historyauflösung oder Ambiguous/Missing. Keine alte Ebene als gültige Ersatzauflösung. Projizierte Kreiskante bleibt trotz geänderter Tessellierung dieselbe analytische Quelle.

## A-REF-03 – Bewusste Reparatur und Legacy-Migration

Alte Index-/Supportreferenz in eindeutigem und mehrdeutigem Modell importieren. Eindeutige Zuordnung migriert; mehrdeutiger Fall zeigt Kandidaten. Benutzer wählt Ersatzfläche: eine Transaktion; Save/Open erhält sie; Undo stellt vorherigen Fehler wieder her. Ein beschädigter Bezug führt nie durch automatisch kleinste Distanz zu einer willkürlichen anderen Fläche.

## A-REG-01 – Konturen, Löcher und Inseln

Fixtures: Rechteck, gemischter Line/Arc-Rand, Kreis, Ellipse, zwei getrennte Rechtecke, konzentrische Kreise R20/r10, drei verschachtelte Kreise, offene Kontur, T-Verzweigung, Selbstschnitt, doppelte Randkante und tangentiale Berührung. Konstruktion durch gültige Region darf keine Materialgrenze erzeugen.

Permutation der Elementliste, umgekehrte Orientierung und nicht monotone IDs ändern Geometrie nicht. Ringregion hat Fläche `π(20²−10²) = 300π mm²`; ausgewählte Innenregion hat separat `100π mm²`. Es werden nicht beide automatisch gefüllt. Verschachtelung erzeugt die explizit gewählten Regionen mit richtigen Innenwires. Ungültige Fälle liefern lokalisierte Rand-/Elementfehler.

## A-REG-02 – Profilidentität bei Änderung

Ringregion auswählen und Extrude anbinden. Reine Radiusänderung erhält die ausgewählte fachliche Region. Elementreihenfolge ändern oder eine irrelevante Konstruktion ergänzen verändert RegionId nicht. Aufteilung/Zusammenführung eines ausgewählten Bereichs liefert die in R09 festgelegte eindeutige Fortführung oder fordert Reparatur. Löschen des kleinsten Element-IDs darf kein anderes Profil auswählen.

## A-SKETCH-01 – Werkzeugmatrix

Für jede unterstützte Familie eine Zeile: Line, Polyline, Rectangle, Circle, Arc einschließlich3-Punkt, Ellipse, Polygon, Slot, Move, Rotate, Mirror, Offset, Trim, Extend, Split, Fillet, Linear/Circular Pattern. Spalten: Vorschau, tatsächliches Erzeugen, Snap/Auto-Constraints, numerische Eingabe, Edit, Cancel, Undo/Redo, Labels/DOF, Save/Open, unterstützte Geometrietypen, Fehlerfall.

Nicht jede Operation muss jeden Kurventyp unterstützen. Aber jede aktive Kombination braucht eine vollständige Zeile; nicht unterstützte Kombinationen werden vor Mutation erklärt. Ellipsenoffset durch Halbachsenaddition besteht diese Abnahme nicht. Mehrfachspiegelung mit Reallokation und Ellipsenmittelpunkt-Translate sind Pflichtregressionen.

## A-SKETCH-02 – Alle Constrainttypen

Aus `SketchConstraintType` und Codec die tatsächliche Typmenge ableiten. Für jeden vorhandenen Typ gültige Beziehung, unzulässige Referenzkombination, Annotation/Einheit, Solve, Edit, Undo und Roundtrip prüfen. Insbesondere Radius/Durchmesser, X/Y-Abstand, Winkel, Konzentrisch, Tangente, Symmetrie und projizierter Bezug. Vollständig bestimmt/unterbestimmt/Konflikt/Failed mit jeweils passender Anzeige. Neues Constraint ohne vollständigen Pfad darf nicht aktiv angeboten werden.

## A-BODY-01 – Mehrkörper und leeres Dokument

Ohne Boxvorlage auf XY eine Skizze und NewBody-Extrusion erstellen. Zweiten unabhängigen Körper auf XZ erzeugen. Zusätzliche Skizze zwischen zwei Solidfeatures einfügen und explizit Körper A ändern. Körper B bleibt unverändert; Zielwahl folgt nicht „letztes Feature“. Namen ändern und Save-As erhalten IDs.

## A-BODY-02 – Platzierung aller Körperarten

Box und skizzenbasierte Extrusion mit identischer Geometrie jeweils um(30,−10,5)mm verschieben und um90° um Z drehen. Boundingbox-/Punktlage vergleichen. Stützskizze, projizierte Kante und anschließender Cut verwenden konsistente Lokal-/Weltkoordinaten. Ein Manipulatordrag erzeugt ein Undo.

## A-EXT-01 – Richtungen und erstes Volumen

Rechteck20×10mm auf XY, Distanz8mm: NewBody `1600 mm³`. Blind positiv ergibt Z-Bereich[0,8], Reverse[−8,0], Symmetric mit Gesamtlänge8 ergibt[−4,4]. Auf XZ/YZ entsprechend transformiert. Ring R20/r10×8 ergibt `2400π mm³`. Offenes Profil, Distanz0/NaN und verlorene Region rejected.

## A-EXT-02 – Join, Cut/Pocket, Intersect

Quader40×30×10. Durchgehender Zylinder r3 entfernt `90π mm³`; rechteckige, vollständig innenliegende Sacktasche10×8×4 entfernt320mm³. Getrenntes Join entsprechend festgelegtem Ein-Solid-Vertrag abweisen oder explizit getrennt ausgeben, aber nicht still als verbunden erklären. Intersect mit disjunktem Tool liefert verständlichen EmptyResult-Status. Preview/Commit stimmen in Volumen und Lage überein.

## A-FILLET-01 – Einfache Verrundung und Fase

Box20×20×10, genau eine äußere vertikale Kante der Länge10 auswählen, Fillet-Radius1 ohne angrenzende Bearbeitungen: entfernte Fläche im Querschnitt `1−π/4 mm²`, Volumenabnahme `10(1−π/4) mm³`. Symmetrische45°-Fase mit Abstand1 entfernt `10/2 = 5 mm³`. Validität und richtige Kante prüfen. Negative und zu große Werte liefern Fehler. Nach maßlicher Basisänderung History-/Repairvertrag prüfen; Multi-Edge-Fillet nicht mit dieser Einzelkantenformel vergleichen.

## A-HOLE-01 – Bohrung

Quader40×30×10, Bohrung Ø6 durchgehend: entfernt90πmm³. Blindtiefe4: entfernt36πmm³, Z-Lage nach definierter Stützfläche. Körperdicke10→20: ThroughAll bleibt durchgehend, Blind bleibt4. Quelle der Bohrposition verändern: Loch folgt; Quellreferenz löschen: lokalisierter Fehler.

## A-MEASURE-01 – Messen ohne Modellmutation

Boxkanten und90°-Winkel, Kreisradius, Ringfläche300πmm² und Extrusionsvolumen2400πmm³ messen. Einheit/rundbare Anzeige getrennt vom internen Wert. Camera/DPI ändern Ergebnis nicht. Fehlerhafte/stale Geometrie wird nicht als aktuelles gültiges Maß ausgegeben. Öffnen/Schließen des Messwerkzeugs setzt kein Dirty.

## A-HISTORY-01 – Frühe Änderung durch komplette Kette

Referenzteil A-PART-01 erzeugen. Grundbreite60→70 ändern, Hole/Pocket/Fillet neu auswerten; unveränderte parameterbasierte Positionen und gültige Referenzen prüfen. Undo stellt vorherige komplette Kette her. Ungültige Änderung erzeugt Previewfehler oder explizit bestätigten reparierbaren Zustand, keine heimliche Löschung nachfolgender Features.

## A-HISTORY-02 – Suppress/Rollback/Reorder/Delete

Pocket unterdrücken, speichern/öffnen und wieder aktivieren; Filletabhängigkeit folgt Vertrag. Rollback vor Fillet, früheres Maß ändern, Rollforward: Features erhalten IDs. Ungültiges Reorder vor benötigten Input rejected. Delete zeigt betroffene Nachfolger und verwendet bewusst gewählte Kaskade/Repair; ein Undo stellt vollständigen Graph wieder her.

## A-STEP-01 – Interoperabilität und eingebettete Quelle

Mehrkörpermodell mit Box und Ring als STEP exportieren und in neues Dokument importieren. Solidanzahl, Units, Boundingbox und Volumen vergleichen; keine Timeline-Wiederherstellung erwarten. Zusätzlich bekannte inches-Datei, beschädigte STEP-Datei und sehr große Eingabe. Native .volund mit Importfeature speichern, ursprüngliche STEP-Datei entfernen/verschieben, .volund erneut öffnen: eingebettete Quelldaten bleiben verfügbar. Exporte aus ungültigem aktuellem Modell werden nicht still aus stale Shapes erzeugt.

## A-UI-01 – Vollständiger Produktweg

Installierte/produktive Shell öffnen → New → XY-Skizze → Rechteck20×10 → Maße/Constraint setzen → Sketch-OK → Extrude8 NewBody → Previewparameter8→12→8 → OK → Feature editieren → Undo/Redo → Save → Close → Open. Tatsächliche Handler, Tree/Timeline, Revision und Volumen1600mm³ prüfen. Aufgesetzte Skizze mit Pocket ergänzen. Keine direkten Test-Backendcalls als Ersatz für Klicks/Fields.

## A-UI-02 – Einheitlicher Panel-Lebenszyklus

Für Sketch/Extrude/Fillet/Chamfer/Hole: starten, gültige Preview, Parameter ändern, Cancel; anschließend identischer Ausgangszustand. Erneut starten und OK: genau eine Transaktion. Escape-/Toolwechselregeln prüfen. Ungültige Skizze → Extrude-Klick startet keinen zweiten widersprüchlichen Editor. Kein verwaister Overlay-/Selectionzustand.

## A-UI-03 – Dokumentwechsel mit ungespeicherten Änderungen

New/Open/Close bei dirty Dokument jeweils Save/Discard/Cancel testen. Failed Save darf nachgelagertes Schließen nicht ausführen. Cancel des Dateidialogs ist kein Fehler und keine Dokumentmutation. Dateiargument aus Desktopzuordnung öffnet dieselbe sichere Open-Aktion. Keine Testautomatik, die Schutzdialoge umgeht.

## A-GL-01 – Rendering-Lebensdauer

Viewport erzeugen, Fit/Projection wechseln, Resize, Minimieren/Maximieren, Reparent soweit Produkt relevant, Kontext zerstören/erneuern, Fenster schließen. Bei100/150/200% DPI und tatsächlichem Bildschirmwechsel prüfen, sofern vorhanden. FBO-Dimension und Picking bekannter Punkte korrekt; keine Redraws gegen inaktiven/fremden Kontext. Loop50× für Ressourcenwachstum messen. Simulation und reale Grafiktests getrennt ausweisen.

## A-VIS-01 – Eigene optische Prüfung eines vollständigen UI-Ablaufs

Aktuelles Produkt mit Referenzteil und fester Kamera öffnen. Genehmigte Gestaltung bzw. letzte geprüfte Baseline angeben. Bei normaler und minimal unterstützter Fenstergröße sowie verfügbaren Skalierungen selbst folgende Zustände bedienen und aufnehmen: ausgewähltes Teil, Skizze mit Maß-/Constraintlabels, geöffnetes Extrude-Panel, gültige Preview, ungültige Eingabe/Fehlermeldung und Zustand nach Cancel.

Gesamtfenster und nötige Details tatsächlich mit einem Bildlesewerkzeug betrachten. Prüfen: Texte/Einheiten vollständig lesbar, Controls erreichbar, keine Überlappungen, richtige Highlights, Handles/Labels an richtiger Stelle, GL-Inhalt und Overlays sichtbar, nach Cancel kein verwaistes Panel/Overlay. Dokument-/Undo-/Dirtyzustand zusätzlich fachlich prüfen. Eine absichtlich abgeschnittene Beschriftung oder verdeckte Aktion wird als Fehler dokumentiert; die Baseline darf dies nicht durch automatische Aktualisierung legitimieren.

Ergebnis enthält Buildrevision/Dirty-Diff, Umgebung, Maße/DPI, Aufnahmemethode, Bildpfade und konkrete eigene Beobachtungen. Fehler beheben und betroffene Aufnahmen wiederholen. Fehlende Bildlese-/Native-Displaymöglichkeit bleibt präzise als Rest offen; bloß erzeugte Bilder zählen nicht als optisch geprüft. Zuständigkeit: Agent, zuerst A08/U11, danach alle betroffenen UI-Pakete.

## A-PLAT-01 – FreeBSD/Meridian

Pro Check Ergebnis/Evidenz: nativer Wayland-QPA; erster OCCT-Frame; Face-/Edgepick; Orbit/Pan/Zoom; Overlays; Resize/Maximize; Keyboard/Fokus; Portal Open/Save; Clipboard; Theme; Skalierung/Multimonitor; Suspend/Resume; GL-Kontextwiederaufbau; Referenzteil erstellen/editieren/speichern/öffnen/exportieren. Build- und Laufzeitversionen sowie Hardware dokumentieren. Sway-Kontrolle getrennt, XWayland kein Ersatz. P04 nimmt gemäß ADR 0007 die FreeBSD/Sway-Entwicklungsbasis ab; L07 führt die abschließende echte Meridian-Integration und die gesamte Liste mit installiertem Releasepaket aus.

## A-RECOVERY-01 – Wiederherstellung

Manuell Zustand A speichern, B ändern, Recovery erzeugen, Prozess abbrechen. Originaldatei lädt A; Recoverydialog bietet B mit Zeit/Revision an. Wiederherstellen und Save-As möglich. Beschädigtes/älteres/fremdes Snapshot sowie zwei Appinstanzen prüfen. Restore/Discard verändert keine fremde Dokumentdatei.

## A-PERF-01 – Belastung mit nachgewiesenen Grenzen

Referenzhardware und Builds dokumentieren. Messreihen beispielsweise100/1.000 Skizzenelemente,10/100/500 Features,1/10/100 Körper, jeweils repräsentative Constraints/Abhängigkeiten statt nur isolierter Linien. Werte sind Laststufen, noch keine behaupteten unterstützten Grenzwerte. Median/p95 Solve/Recompute/UI-Reaktion, Peak-/Restmemory nach Open/Close und Storezeit erfassen. Produktbudgets aus Messung und gewünschtem Arbeitsfall vor Release festlegen. Zu große Eingaben vor Ressourcenerschöpfung abweisen; abgebrochene/veraltete Auswertung darf keine neuere Revision überschreiben.

## A-PART-01 – Mechanische Referenzplatte

1. XY-Rechteck von(−30,−20) bis(30,20), Extrude10 in +Z: Grundkörper60×40×10.
2. Auf oberer Fläche Durchgangsbohrung Ø8 bei(12,0), Achse Z.
3. Auf oberer Fläche Rechtecktasche von(−24,−10) bis(−10,10), Tiefe4 nach innen. Abstand zur Bohrung und zu Außenrändern bleibt positiv.
4. Genau eine äußere vertikale Kante bei(−30,−20), Radius1 verrunden. Auswahl über semantische Referenz, nicht Index.
5. Speichern, schließen, öffnen; Breite ändern, Undo/Redo, Pocket suppress/unsuppress, Fillet editieren.
6. STEP exportieren/importieren; Volumen/BBox/Solidanzahl vergleichen.

Volumenstufen:24000mm³ → `24000−160π` → `22880−160π` → `22880−160π−10(1−π/4)`. Einzelkantenannahme aus A-FILLET-01 beachten. Jede Stufe gültiger Solid; keine Shapequalität nur aus finalem Zahlenwert ableiten.

## A-PART-02 – Ring und unabhängiger Körper

Zwei konzentrische Kreise R20/r10 auf XY, explizit Ringregion extrudieren8; Volumen2400π. Unabhängige Box10³ bei deutlich getrenntem X-Bereich erzeugen. Außenradius20→24 ergibt3808π, Box unverändert. Region bewusst durch zusammenfallende Radien ungültig machen: Ringfeature Failed, Box Valid, kein zufällig gefüllter Kreis. Fehlerzustand speichern/öffnen und reparieren. Dies verbindet Lochprofil, DAG, Mehrkörper, Persistenz und Repair in einem Modell.

## Gateprotokoll

Jeder Gatebericht enthält: Commit, Datum, ausführende Umgebung, Paketliste, Szenarioresultate, Befehle/Logs, Artefakte, bekannte Grenzen, externe offene Voraussetzungen und Freigabeentscheidung. `passed`, `failed`, `not-run` und `blocked-external` getrennt halten. Geplante Tests in diesem Dokument sind keine Evidenz; erst reale Resultate schließen Pakete.
