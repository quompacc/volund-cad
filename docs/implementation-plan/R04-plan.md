# R04 – Stützflächenbindung in prüfbaren Teilblöcken

17.09.2026. R04 bleibt ein Hauptpaket; Teilblöcke ändern die Gesamtzahl nicht.
Voraussetzungen R03 und D10 sind abgenommen.

**R04 insgesamt done**, 17.09.2026. [Gesamtabnahme](evidence/R04.md):
84/84 Windows, 81/81 FreeBSD, 341 identische Quell-/Fixture-Hashes und eigene
UI-Sichtprüfung beider Plattformen.

## R04a – Herkunft im ausgewerteten Featuregraphen

Status: done als Teilblock. [Nachweis](evidence/R04a.md): 82/82 Windows,
79/79 FreeBSD; 332 identische Quell-/Fixture-Hashes.

Primitive-Rollen aus R03 durch Platzierung und Körperoperationen führen; pro
EvaluatedFeature die tatsächlichen Nachfolger einer bekannten Herkunft halten.
Cacheübernahme und Fehlerisolation müssen dieselben unveränderlichen Ergebnisse
erhalten. Eindeutige, gelöschte, geteilte und unbekannte Fortsetzung unterscheiden.
Keine neue persistente Referenz, keine Änderung des SketchSupportResolver und
keine Abnahme von A-REF-01/02. Neue Extrusionsflächen bleiben ausdrücklich unbenannt.

## R04b – Dauerhafte Bindung und fachliche Auflösung

Status: done als Teilblock. Dauerhafte Bindung, Schema 2 mit Schema-1-
Lesekompatibilität, Herkunftsauflösung und benannte Extrusions-Endflächen sind
abgenommen: 83/83 Windows, 80/80 FreeBSD, 339 identische Quell-/Fixture-Hashes
und erfolgreicher beidseitiger Dateiaustausch. [Nachweis](evidence/R04b.md).

- Den R02-Vertrag in dokumentseitige SubshapeRef-/SupportBinding-Felder überführen:
  Dokument, Körper, Ursprung und Rolle; gültige lokale Skizzenbasis relativ zur
  Herkunft. Zielstand/aktueller Produzent bleiben im Auswertungskontext.
- Den Erzeugungsweg mit eindeutig ausgewählter realer Fläche binden. Ein Ursprung
  mit mehreren Nachfolgern darf nicht allein nach Normale, Distanz oder Index
  aufgelöst werden. Ein Neuanlegen nach Split benötigt einen ausdrücklich stabil
  benennbaren Kandidaten oder wird mit Grund abgewiesen.
- Ursprungs-/Rahmenfortführung für unterstützte Extrusionsflächen konkretisieren;
  Regionsänderungen ohne belegte Identität bleiben reparaturbedürftig, R09 wird
  nicht durch heutige ProfileId ersetzt.
- Neue Felder über die vorhandene OCAF-Autorität, Snapshots, Undo/Redo und native
  Persistenz führen. Das erfordert bereits hier eine explizite Schemaerweiterung
  mit kompatiblem Lesen des bisherigen Schemas; konkrete Nummer beim Start prüfen.
  Keine flüchtige Cache-ID als scheinbar dauerhafte Dokumentbindung einführen.
- Normale gebundene Skizzen ausschließlich anhand Herkunft und aktueller Fläche
  auswerten. Rotation trägt Ursprung/Basis; fehlende oder mehrdeutige Fläche erzeugt
  einen lokalisierten Featurefehler. Letzte Ebene nur als markierte Reparaturvorschau.

R06 behält die automatische Legacy-Migration, Kandidatenauswahl und bewusste
Reparaturtransaktion. Vorziehen der Speicherung neuer Referenzen in R04b ist nötig,
damit R04 keine Bindung liefert, die bei Save/Open verloren geht. Neue native
Schemafelder müssen auf beiden OCCT-Versionen geprüft werden; Originaldateien
bleiben unverändert bis Save. Altmodelle sind explizit als ungebundener Legacy-Pfad
zu unterscheiden, nicht stillschweigend als neue stabile Referenz zu behandeln.

## R04c – Produktanbindung und Gesamtabnahme

Status: done. Produktanbindung und [Abnahme](evidence/R04c.md) abgeschlossen.

Viewport-Picking, SketchEditor und Extrusion lesen dieselbe application-/features-
Projektion. Alte doppelte Stützflächenheuristik im regulären gebundenen Pfad ablösen;
projizierte Segmentreferenzen separat als R05-Rest ausweisen.

A-REF-01/02 am Produkt prüfen: zwei parallele/koplanare Flächen nicht vertauschen,
Rotation führt die Skizze mit, gelöschte Fläche bleibt sichtbar fehlerhaft, ihr
abhängiger Zweig blockiert und unabhängige Körper bleiben gültig. Zusätzlich
Save/Open, Undo/Redo, Ablehnung veralteter Kandidaten und eigene UI-Sichtprüfung.
Windows/FreeBSD mit identischen Quellen abnehmen. Erst danach R04 auf done setzen.
