# A03-R1 – Langloch-Endpunkte erhalten die gewählten Maße

Bei der A03-Gesamtabnahme scheiterte `application.sketch_basic_tools` sporadisch.
Eine Wiederholung scheiterte nach sechs erfolgreichen Läufen. Die ergänzte
Diagnose zeigte kollabierte Langlochkappen trotz Solverstatus `success`.

Die Linien eines Langlochs werden in Konturrichtung durchlaufen, die Halbkreise
sind gegenläufig parametrisiert. `confirmPreview()` verband zuvor pauschal
`end` mit `start`. Beim Langloch wurden dadurch gegenüberliegende statt
geometrisch zusammenfallende Endpunkte gleichgesetzt. Die Endpunktwahl ist nun
für die beiden Halbkreise umgekehrt; Polygone behalten ihre bisherige Zuordnung.

`application.sketch_slot_dimensions` prüft horizontale, vertikale, umgekehrte
und schräg liegende Langlöcher: Bestätigung erhält Mittelpunkte, Radius 3 und
Halbkreiswinkel π; alle erzeugten Koinzidenzen sind geometrisch erfüllt und
der Solver meldet Erfolg. Die geometrischen Residuen verhindern einen falschen
Erfolg, wenn ein fehlgeschlagener Solve die unveränderte Vorschau stehen lässt.

Vor der Korrektur wurde der neue Test rot ausgeführt: Radien etwa 0,00005055
statt 3. Nach der Korrektur bestanden dieser Test und `sketch_basic_tools`
je 30 aufeinanderfolgende Läufe im Produktbuild.

Dies ist ein separat dokumentierter Fehlerfix neben der verhaltenserhaltenden
A03-Dateiaufteilung. Allgemeine Weitergabe fehlgeschlagener Solverresultate
bleibt D07; dieser Fix ersetzt dieses Paket nicht. Bestehende gespeicherte
Langlöcher werden durch diesen Erzeugungsfix nicht migriert oder repariert.

Evidenz und Bilder: [A03](../../docs/implementation-plan/evidence/A03.md).
