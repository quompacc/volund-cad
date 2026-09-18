# R05b-P – Persistente Kantenreferenzen und analytische Constraintauflösung

Aktueller Abschluss 18.09.2026: [R05 insgesamt abgenommen](R05.md). Der folgende Teilbericht dokumentiert seinen damaligen Prüfstand.

18.09.2026. R05b-P plattformübergreifend abgenommen; UI-Auswahl folgt danach, R05 bleibt offen.

## Umsetzung und Gegenprüfung

P1 verschiebt Primitive-/Konturherkunft in die Dokument-Datentypen. Die bestehende
Feature-Provenienz verwendet dieselben Typen per Alias. SketchElementId erhält einen
kleinen separaten Header, um den Zyklus zwischen Geometrie und Kantenreferenzen zu vermeiden.
`ProjectedEdgeReference` enthält Dokument-/Körpernamespace, Herkunft und Anfang/Ende
oder einen normierten analytischen Parameter. Endpunktauswahl speichert Parameter 0.
Ein neues optionales Constraintfeld ist nur für unäre X/Y-Koordinaten zulässig;
Mischung mit alten numerischen Segmentattachments wird abgewiesen.

P2 schreibt Anwendungsschema 3 im unveränderten OCAF-Container 12. Schemas 1/2
bleiben lesbar, neue Constraint-Kinder unter diesen Versionen werden abgewiesen.
Feste Payloadgröße, kanonische IDs, gültige Rollenpaare, Konturrollen/-stufen,
endliche Parameter und konsistenter Namespace werden geprüft. Writer validiert vor
nativer Mutation; Reader führt dieselben Fachprüfungen aus. Neue Referenzen setzen
eine ausdrücklich gebundene Stützfläche voraus. Dokumentduplikate remappen auch
die internen Kantenreferenzen; normale Revision/Save/Open erhalten sie.

P3 löst die aktuelle analytische Kante aus der Provenienz auf, prüft echte
Topologie-Mitgliedschaft in der benannten Stützfläche und projiziert den Punkt in
den aktuellen Skizzenrahmen. Erst danach löst der Solver die Constraints. Fehlend,
mehrdeutig, unbelegte History/Parameterabbildung und abgeschnittene Parameter erzeugen
Fehler; gespeicherte Koordinaten sind kein Fallback. Extrusions-Referenzgeometrie
verwendet ihre eigenen Kanten und löst die zugehörige Referenzskizze erneut.

Keine neue Datenautorität, kein zweiter Featurebaum, keine automatische Legacy-Reparatur.
Die Referenz bezieht sich auf eine Kante des bereits deklarierten Stützfeatures;
es wird kein versteckter zusätzlicher DAG-Eingang eingeführt. Eine fehlende Herkunft
bleibt als reparaturbedürftiger Dokumentinhalt speicherbar; fremde Namespaces werden
bereits an der Dokumentgrenze zurückgewiesen. Legacy-Migration bleibt R06.

## Prüfung P4

Neuer Test `features.projected_edge_reference` prüft Primitive-/Kontur-Roundtrips,
Dokument-/Körpernamespace, Duplizieren, Größen-/Platzierungsänderung, analytische
Kreisparameter, Undo/Redo, isolierten Fehler und blockierte Nachfolger. Zusätzlich
Schema-1/2-Lesekompatibilität, Schema-2-Stützflächen ohne neue Felder, Schema-3-Gating,
Endpunktarten, ungültige IDs/Parameter und gemischte alte/neue Attachments.

Vollständiger Windows-Build und fokussierter Referenztest bestanden.
Quellgrößenprüfung: 332 Dateien bestanden. Diffprüfung bestanden.
Vollständige Windows-Suite: **87/87 Tests bestanden**, 116,42 s. [JUnit](R05b-P-artifacts/windows-ctest.xml), [Log](R05b-P-artifacts/windows-ctest.log). FreeBSD: **84/84 Tests bestanden**, 30,96 s. [JUnit](R05b-P-artifacts/r05bp-ctest.xml), [Log](R05b-P-artifacts/r05bp-ctest.log).
353 Quell-/Fixture-Hashes, 22 geänderte/neue Dateien gegenüber dem abgenommenen Konturteil.

Noch keine neue Auswahl im Viewport, kein neuer Editor-Add-Befehl. Diese Produktanbindung
und die abschließende R05-Abnahme sind der folgende Schritt. Kein Commit/Push.

Nach anfänglicher automatischer Ablehnung hat der Nutzer den konkreten R05b-P-Transfer mit „weiter“ freigegeben. FreeBSD-Build und Prüfskript erfolgreich (Exit 0). Alle 353 Quell-/Fixture-Hashes und drei Referenzfixture-Ausgaben sind identisch. Die Windows-Datei wurde unter FreeBSD gelesen; die dort geschriebene Datei unter Windows erfolgreich geprüft. [Austauschprüfung](R05b-P-artifacts/exchange-verification.txt).
