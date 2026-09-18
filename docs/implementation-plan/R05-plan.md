# R05 – stabile projizierte Kanten

17.09.2026. R05 ist ein Hauptpaket; Voraussetzung R04 ist abgenommen.
Zwei Blöcke trennen den Nachweis analytischer Kantenidentität von den darauf
aufbauenden persistenten Constraints und dem Bedienweg.

## R05a – Primitive-Kantenherkunft und analytische Parameter

Status: abgeschlossen; 85/85 Windows, 82/82 FreeBSD und 344 identische Quell-/Fixture-Hashes. [Nachweis](evidence/R05a.md).

- Boxkanten über das Paar angrenzender semantischer Primitive-Flächen benennen.
  Anfang/Ende über die negative/positive Fläche der freien lokalen Achse belegen.
- Zylinder-Endkreise über Deck-/Mantelrolle benennen; Kreisnaht und Richtung aus
  dem analytischen Primitive-Rahmen übernehmen. Keine erfundene Seam-Kantenrolle.
- Bekannte Kanten durch echte Boolean-History weiterführen; Input und Tool trennen.
  Unveränderte analytische Kurve inklusive Location belegt die Parameterabbildung
  bei Trim. Geometrische Ähnlichkeit allein genügt nicht.
- Normierter Parameter bleibt auf dem ursprünglichen Quellintervall; fehlender
  Parameter auf Reststücken wird nicht geklemmt. Split, Löschung und fehlende
  History unterscheiden. Tessellierung liefert Herkunft plus analytische Intervalle.
- Featurecache, Platzierung, Recompute und Undo/Redo mit echten OCCT-Kanten prüfen.

R05a benennt noch keine neuen Extrusionskonturkanten und erzeugten Schnittkanten;
bestehende Primitive-Kanten werden durch Extrusion fortgeführt. Keine neue
Constraint-Persistenz oder UI-Freischaltung. Schema 2 bleibt unverändert.

## R05b – Konturkanten, persistente Constraints und Produktabnahme

Status: abgeschlossen, 18.09.2026. [Gesamtabnahme](evidence/R05.md); historische Teilnachweise unten.
Fortsetzung 18.09.2026: abgenommener Prüfschritt [R05b-P](evidence/R05b-P.md) umfasst P1 (typisierte Daten
und Validierung), P2 (Schema 3 und Namespace-Erhaltung), P3 (analytische
Constraintauflösung) und P4 (Persistenz-/Recompute-/Austauschabnahme). Die anschließende
UI-Auswahl ist ein eigener Prüfschritt. Keine gleichzeitige Freischaltung ungeprüfter
Referenzen; mehrdeutige oder fehlende Kanten bleiben Fehler.
Erster Prüfschritt abgenommen: Erzeugerherkunft für Konturen und Extrusions-Anfang/Ende,
einschließlich getrennter Welt-/Referenzgeometrie. 86/86 Windows, 83/83 FreeBSD,
347 identische Quell-/Fixture-Hashes. Anschließend R05b-P mit Schema 3 und persistenten Constraints abgenommen: 87/87 Windows, 84/84 FreeBSD, 353 identische Hashes und beidseitiger Dateiaustausch. UI-Anbindung mit R05b-U ebenfalls plattformübergreifend abgenommen.

- Für unterstützte Skizzenkonturen Element-ID und feste Kurven-/Rechteckseitenrolle
  am tatsächlichen Erzeugungsschritt festhalten; Extrusionskanten anhand echter
  Prism-History benennen. Kein Explorerindex als Ersatz. Neue Schnittkanten nur
  bei belegtem Erzeugervertrag; unbekannte Kandidaten explizit nicht auswählbar.
- Dokument-/Körpernamespace und Kantenherkunft mit Anfang/Ende oder normiertem
  analytischem Parameter als typisierte projizierte Referenz speichern. Native
  Schemaerweiterung ausdrücklich versionieren; bestehende Dateien lesbar halten.
- Constraintauflösung, Auswahl und Darstellung aus gemeinsamer Feature-/Application-
  Projektion beziehen; reguläre Viewport-Heuristik entfernen. Kurven analytisch
  auswerten, Anzeige tessellieren. Fehler isolieren und bewusste Reparatur R06 lassen.
- Dichtewechsel, davor eingefügte Kanten, Recompute, Kreisnaht/Richtung,
  Save/Open, Duplizieren, Undo/Redo sowie Produktbedienweg auf Windows/FreeBSD prüfen.

Erst R05b schließt R05 ab. R06 behält Legacy-Migration und explizite Reparatur;
R09 allgemeine Regionsidentität. Keine allgemeine Projektionsfläche jenseits
planarer Skizzen in v1.

Abschluss R05b-U: [Editor-Anbindung](evidence/R05b-U.md) mit 88/88 Windows- und
85/85 FreeBSD-Tests, 358 identischen Quellhashes und beiden UI-Sichtprüfungen
abgenommen. R05b und R05 abgeschlossen; R06 folgt.
