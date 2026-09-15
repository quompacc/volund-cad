# 02 – Zielverträge für die Umsetzung

Diese Verträge konkretisieren den angenommenen Architekturvertrag. **Neu vorgeschlagene Entscheidungen** sind ausdrücklich markiert und werden in den angegebenen Entscheidungspaketen durch Headerentwürfe und kleine Nachweise abgeschlossen. Ein ausführendes Modell darf keine alternative Gesamtarchitektur nebenbei einführen.

## K01 – Schichten und Eigentum

| Schicht | Besitzt / entscheidet | Darf nicht |
|---|---|---|
| `document` | Dokumentidentität, Featureparameter, Referenzen, Persistenzschema, Transaktionshistorie | QWidget kennen; Darstellungszustand als Modellwahrheit speichern |
| `kernel` | OCCT-Operationen, Shape-Prüfung, Operation History, Kernel-Fehlerübersetzung | UI öffnen; Dokument heimlich mutieren |
| `features` | Parametrische Auswertung, Abhängigkeiten, Regionen/Referenzauflösung | Letzte gültige Geometrie als erfolgreichen neuen Zustand ausgeben |
| `application` | Befehle, Bearbeitungssitzungen, Commit/Cancel, Dateianwendungsfälle, Ergebnisprojektionen | Schaltflächen gestalten; globale veränderliche Services halten |
| `viewport` | Darstellung, Picking, Kamera, Eingabe in Anwendungsereignisse übersetzen | Featureparameter direkt schreiben; eigene fachliche Referenzauflösung betreiben |
| `desktop` | Eine produktive Shell, Panels, Registry-Bindung, verständliche Meldungen | Geometriealgorithmus oder zweite SketchEditorSession-Implementierung werden |
| `platform` | Dateiersatz/Durability, native Integration, Betriebssystemadapter | Dokumentfachlogik übernehmen |
| Workspace-Module | Registrierte Befehle und Panels für ihren Fachumfang | Kernoperationen duplizieren; inaktive Attrappen als verfügbare Befehle anbieten |

Kleine zustandsarme DTOs dürfen an den Grenzen stehen. OCCT ist im bestehenden Code sichtbar; die Entkopplung erfolgt schrittweise, nicht durch einen pauschalen Tausend-Zeilen-Umbau. **Vorschlag:** opaker `ShapeHandle` für veröffentlichte Auswertungsergebnisse; OCCT-Zugriff bleibt im Kernel und in einem klar bezeichneten Darstellungsadapter. Keinen generischen Frameworkbau vor konkretem Nutzen beginnen.

## K02 – Identitäten, Einheiten und Toleranzen

**Vorschlag für D10/D11:** `DocumentId`, `BodyId`, `FeatureId`, `ElementId`, `ConstraintId`, `RegionId`, `SubshapeRef` sind unterschiedliche Typen. Körperidentität bleibt bei weiteren Features konstant; Featureidentität bezeichnet eine bestimmte Erzeugung/Änderung. Umbenennung, Verschieben der Datei und Reihenfolgeänderung verändern die fachliche Identität nicht. IDs werden nicht wiederverwendet; Überlauf wird abgefangen. Dokumentduplikat und Revision bekommen explizit unterschiedliche Semantik.

Alle internen Längen in Millimeter, Winkel in Radiant; Umrechnung an Eingabe-/Dateigrenzen. Ebenen werden durch validierten Ursprung und orthonormale rechtshändige Basis beschrieben. NaN/Inf, degenerierte Achsen und unzulässige Maße werden vor Mutation abgewiesen. Unterschiedliche Darstellungspräzision verändert keine Modellmaße.

**D10b1-Ebenenentscheidung:** Produkteingaben müssen bereits orthonormal sein (64 * double-epsilon als dimensionslose Rundungsgrenze); keine automatische Normalisierung. Historische nicht kanonische Ebenen bleiben verlustfrei les-/speicherbar, werden im Importbericht markiert und vor Geometrieauswertung als Fehler abgewiesen. Explizite Korrekturen sind normale geprüfte Befehle mit Undo/Redo. Die einheitliche interne Radiant-Konvention ist mit [D10b2b](evidence/D10b2b.md) umgesetzt; Legacy- und UI-Gradgrenzen konvertieren explizit.

[D10c](evidence/D10c.md) definiert getrennte Typen für Verbindung (mm), Winkel (Radiant), Modellvergleich (relativ) und Picking (Pixel), ohne global mutable Settings. Längen liegen in [0,001; 1.000.000] mm, Koordinaten betragsmäßig höchstens bei 1.000.000 mm. Die Verbindungsschwelle ist max(OCCT Precision::Confusion(), 1e-12 * lokaler Koordinatenmaßstab). Ungültige Altmaße werden konkret diagnostiziert, nicht korrigiert. [D10d](evidence/D10d.md) ersetzt den pauschalen Extrusionsüberstand durch exakten Supportflächenanschluss. D10 ist nach dem ID-Grenznachweis D10e auf Nutzerauftrag abgeschlossen. Der Beginn von D11 ist in [D11a](evidence/D11a.md) dokumentiert; dessen Zustands- und Persistenzintegration bleibt offen.

**D11-Zwischenstand:** [D11b](evidence/D11b.md) erhält den Feature-Vergabezähler
über Undo hinweg; der Inhaltsvergleich für Dirty schließt diesen Zähler aus.
[D11c](evidence/D11c.md) trennt die dokumentlokale Körper-ID im Auswertungsergebnis
vom letzten Shape-Erzeuger. Für den bestehenden Legacy-Pfad ist ihre Herkunft
aus gespeicherten Feature-IDs und Modi rekonstruierbar. Der Zylinder reaktiviert
bei Separate seine reservierte eigene Herkunft; Join/Cut/Intersect gehören zum
Eingabekörper. Die persistente DocumentId und der native Übergang bleiben offen.

[D11e](evidence/D11e.md) konkretisiert die Dokumentidentität an der unveränderlichen
Store-Grenze: Betriebssystemzufall erzeugt einen nicht leeren 128-Bit-Namensraum.
Wertkopie und Revision erhalten ihn; explizites Duplizieren erzeugt einen neuen,
von der Quelle verschiedenen Namensraum und erhält dokumentlokale IDs. Native
Open übernimmt die gespeicherte ID, Save-As soll sie erhalten. Die letzte Aussage
ist ein Integrationsvertrag, noch kein produktiver Persistenznachweis. Die ID ist
kein Pfad-/Namens-/Inhaltshash und keine Revisionsnummer; es gibt keine UUID-Behauptung.

## K03 – Befehle und Transaktionen

D04 konkretisiert diesen Vertrag im [geprüften Schnittstellen- und Migrationsentwurf](../command-transaction-contract.md). Die produktive Anbindung folgt D05/D06.

Eine Benutzeraktion erzeugt höchstens eine Dokumenttransaktion. Ein Drag umfasst Pointer-down bis Pointer-up; Mausbewegung und Renderrefresh erzeugen keine Undo-Einträge. Erstellen einer Skizze und ihrer Extrusion kann entweder zwei **sichtbar getrennte bestätigte Werkzeuge** sein oder ein zusammengesetzter Befehl; im zweiten Fall muss es genau ein Undo sein. Bestehende Tests, die zwei implizite Schritte festschreiben, werden mit begründeter Semantikänderung ersetzt.

Vorgeschlagener Ablauf:

1. Befehl erhält expliziten Dokument-/Auswertungsstand und validierte Parameter.
2. Bearbeitungssitzung erzeugt Kandidaten; Preview darf mehrfach auswerten.
3. Kandidat wird validiert und auf seiner betroffenen Abhängigkeitsmenge ausgewertet.
4. OK übernimmt Parameter, Identitäten und Fehlerstatus zusammen in genau einer Transaktion.
5. Cancel/Exception/ungültige Eingabe verwirft die Sitzung und stellt auch Auswahl, Solve-Status und temporäre Referenzen wieder her.

**Zwei ausdrücklich unterschiedliche Commitfälle:** Neue ungültige Geometrie wird nicht still als erfolgreiches Feature angelegt. Bei der Änderung eines bestehenden parametrischen Modells darf ein Benutzer einen reparierbaren Fehlerzustand ausdrücklich behalten; UI zeigt „Änderung mit Folgefehlern übernehmen“ und listet betroffene Features. Solche Dokumente müssen speicherbar bleiben. Der Befehl liefert `CommittedWithErrors`, nicht `Success`. Keine zusätzliche generische Genehmigungsfrage für normale gültige Bearbeitungen.

Dirty-State hängt am gespeicherten Dokumentzustand bzw. der gespeicherten Revision, nicht nur an einem booleschen Klickflag. Save → Änderung → Undo zurück zur Save-Revision ist wieder clean. Auswahl/Kamera verändern Dirty nicht; neue Featureparameter tun es. Undo/Redo stellt fachliche Zustände deterministisch wieder her und löst keine neue History-Aufzeichnung aus.

## K04 – Tool-Lebenszyklus und UI-Projektion

Zustände: `Idle → Editing → PreviewReady/Invalid → Committing → Idle`; Cancel führt aus jeder noch nicht bestätigten Bearbeitung nach Idle. Ungültige Skizze beim Wechsel zu Extrude hält die Bearbeitung offen und erklärt den Grund. Es darf nicht trotzdem ein Extrude-Panel entstehen.

Ein gemeinsamer Controller pro Fachwerkzeug; Ribbon, Shortcuts und Kontextmenü lösen denselben registrierten Befehl aus. `ToolPanelHost` besitzt höchstens eine aktive Sitzung und verbindet deren Parameter/Fehler/OK/Cancel. Widgets besitzen keine zweite Kopie der Fachregeln. Nur umgesetzte Befehle sind aktiv. Ein späterer Workspace kann klar als nicht verfügbar gekennzeichnet sein, aber nicht scheinbar funktionsfähig navigieren.

Tree/Timeline/Properties/Skizzenmaße lesen eine versionierte Dokument- und Auswertungsprojektion. Keine fest verdrahteten Featurelisten und keine Anzeige ungeprüfter Originalparameter als bereits gelöste Geometrie. Auswahl ist typisiert und referenziert Identitäten; verschwundene Objekte werden sichtbar ungültig oder deselektiert.

## K05 – Fehler und Auswertung

**Vorschlag:** Ergebnisse enthalten Status, stabile Fehlerkennung, betroffene IDs, technische Details für Logs und eine verständliche UI-Nachricht. C++20 benötigt dafür kein `std::expected` aus C++23 und keine zusätzliche Bibliothek; ein kleiner vorhandenen Konventionen folgender Result-Typ genügt.

Kernelgrenzen übersetzen `Standard_Failure` sowie bekannte Standardfehler. Keine Exceptions quer durch Qt-Callbacks. Programmierfehler nicht pauschal als „ungültige Geometrie“ verschlucken. Auswertungszustände: `Valid`, `Dirty`, `Failed`, `BlockedByDependency`, `Suppressed`. Der letzte gültige Shape darf als gekennzeichnete Vorschau erhalten bleiben, aber nicht ohne Kennzeichnung exportiert oder als aktuelles Ergebnis vermessen werden.

Feature-Abhängigkeiten bilden einen DAG; Timeline-Reihenfolge allein bestimmt sie nicht. Ein Fehler blockiert Nachfolger dieses Knotens, nicht unabhängige Körper. Zyklen werden vor Commit erkannt. Jede Auswertung veröffentlicht ein konsistentes Ergebnis mit Dokumentrevision. Spätere asynchrone Ergebnisse werden verworfen, wenn ihre Revision nicht mehr aktuell ist.

## K06 – Stabile Topologiereferenzen

**Vorschlag für R02:** `SubshapeRef` enthält Dokument-/Körper-/Erzeuger-ID, erwartete Topologieart und einen semantischen Herkunftsschlüssel. Beispiel: Extrusionsdeckfläche einer Region, Mantelfläche einer bestimmten Konturkante. OCCT-History `generated/modified/deleted` wird pro Operation ausgewertet. Geometrische Signaturen dienen zur Plausibilisierung oder expliziten Reparaturvorschlägen, nicht als stiller Ersatz für Identität.

Auflösung liefert `Resolved`, `Missing` oder `Ambiguous` mit Kandidaten. Gleichartige koplanare Flächen dürfen nicht willkürlich vertauscht werden. Eine alte Segmentnummer im Serialisierungsfeld ist kein dauerhafter Kantenschlüssel. Aufteilungen/Zusammenführungen müssen eine deterministische dokumentierte Regel oder einen Reparaturzustand erzeugen. Der Benutzer kann eine neue Fläche bewusst zuweisen; diese Reparatur ist eine eigene Undo-Transaktion.

OCAF/TNaming unterstützt Historie und Identitäten, löst aber nicht automatisch sämtliche Topological-Naming-Probleme. Operationstypen benötigen eigene Herkunftsregeln und Regressionen. Die [OCCT-OCAF-Dokumentation](https://dev.opencascade.org/doc/occt-7.9.0/overview/html/occt_user_guides__ocaf.html) beschreibt Dokumente, Attribute, Transaktionen und Naming als zusammenhängende Infrastruktur. R03–R06 müssen die tatsächlichen Fälle beider unterstützter OCCT-Versionen beweisen.

## K07 – Skizzen und Regionen

Eine Skizze darf offen sein. Extrude/Pocket braucht explizit ausgewählte gültige Regionen; die App darf nicht aus zufälliger Elementreihenfolge die gewünschte Fläche erraten. Konstruktionselemente erzeugen keine Materialgrenzen. Eine Region hat Außenkontur und null oder mehr Innenkonturen. Verschachtelung und gemeinsame Grenzen werden deterministisch behandelt; Selbstschnitt und nicht unterstützte Kurvenkombinationen liefern lokalisierte Fehler.

RegionId ist nicht dauerhaft die kleinste Element-ID. R09 legt eine reproduzierbare Zuordnung anhand stabiler Grenzreferenzen fest. Wird eine Region gesplittet, ist ein bisheriger Extrusionsbezug entweder eindeutig fortgeführt oder reparaturbedürftig. Es gibt keine automatische zufällige Auswahl.

Jede Erzeugungsoperation liefert semantische Rollen: Mittelpunkt, Start/Ende, Rechteckecken, Slot-Kappenmittelpunkte usw. Auto-Constraints verweisen darauf statt auf Einfügereihenfolge oder `id + 3`. Ellipse besitzt kein Linien-Startpunktfeld. Arc-Orientierung und Parameterrichtung sind explizit.

Solve-Zustände mindestens `UnderConstrained`, `FullyConstrained`, `Conflicting`, `Failed`; ein numerischer Fehler ist nicht „unterbestimmt“. Die Anzahl der Freiheitsgrade und Konflikt-IDs stammen aus demselben Solve-Ergebnis wie die angezeigte Geometrie. Undo/Cancel stellt den gesamten Editorzustand her. Offset bedeutet geometrisch konstanten Normalabstand; wenn der Datentyp eine Kurve nicht darstellen kann, wird die Operation mit Grund abgewiesen, nicht durch eine andere Kurve angenähert und als exakt bezeichnet.

## K08 – OCAF und Dateiformat: vorgeschlagener Übergang

**D08-Entscheidung vom 14.09.2026:** Der isolierte Austausch 8.0.1 ↔ 7.9.3 ist
einschließlich Rücklauf bestanden. [ADR 0008](../adr/0008-ocaf-binary-persistence.md)
wählt native BinOcaf-Streams mit expliziter OCAF-Schreibversion 12 für D12/D13.
Der Produktcodec ist noch nicht umgestellt; der nachfolgend beschriebene neutrale
Fallback bleibt ein Rückfallweg, ist aber nicht der gewählte neue Writer.

Die OCAF-Anforderung bleibt bestehen. **Vorgeschlagene Zielrichtung:** OCAF als alleinige autoritative Quelle für Featureparameter/IDs/Transaktionen, gekapselt hinter `ModelDocument`; immutable DTO-Snapshots für Anwendung und Legacy-Konvertierung. Kein synchron zu haltendes zweites veränderliches Vektordokument. Cache und Darstellung bleiben abgeleitet.

**D08 ist ein begrenztes Entscheidungsexperiment, kein Blankoscheck für einen Rewrite:** Standard-Attribute und NamedShapes eines Minimaldokuments auf OCCT 7.9.3 und 8.0.1 speichern/lesen, Undo/Redo prüfen, benötigte Module exakt ermitteln. Bevorzugt native OCAF-Binärpersistenz über zugelassene OCCT-Treiber, eigenes Schemasversionsattribut am Dokumentroot und weiterhin Dateiendung `.volund`. Kein selbst erfundenes ZIP-/JSON-Framework und keine neue Bibliothek.

Die Entscheidung muss nachweisen, dass beide Zielplattformen die gewählte Schreibversion lesen können. Falls native Persistenz daran scheitert, **OCAF in-memory beibehalten und den bestehenden neutralen Snapshot-Serializer als Austausch-/Persistenzgrenze weiterführen**, bis eine gemeinsame native Schreibversion bewiesen ist. Dies ist ein benannter Fallback des Entscheidungspakets; nicht zwei parallel veränderliche Wahrheiten. D08 liefert vor D12/D13 eine einzige gewählte Speicherrichtung mit Headern, Modulmatrix und Testbelegen.

Die alten Leser für VOLUND1–VOLUND16 bleiben erhalten; vorhandene Dateien werden ohne automatisches Überschreiben importiert. Neue Versionen werden ausdrücklich erkannt; unbekannte zukünftige Schemata werden nicht geöffnet und beschädigend zurückgespeichert. Neue Formatnummer erst nach Entscheidung und Prüfung des dann aktuellen Hauptzweigs vergeben; historische Werte nicht verändern. Shape-Caches gelten nicht als Ersatz für editierbare Featureparameter. Importierte nichtparametrische BRep-Geometrie ist dagegen Quelldateninhalt eines Importfeatures.

Schreibprotokoll unabhängig vom Codec: Snapshot/Validierung → temporäre Geschwisterdatei → vollständig schreiben → Flush/Close prüfen → betriebssystemspezifischer atomarer Ersatz → erst danach Save-Revision setzen. Wo das Betriebssystem keine zugesicherte Stromausfallsicherheit bietet, Reichweite der Garantie dokumentieren. Fehler vor Ersetzung müssen die bisherige Zieldatei bytegenau erhalten. Recovery nutzt getrennte Dateien und verändert den Benutzerpfad nicht ohne bestätigten Save-Befehl.

## K09 – Viewport und Plattform

Qt besitzt den Kontext. Renderanforderungen gehen über `update()`; unmittelbare GL/OCCT-Redraws außerhalb der vorgesehenen Qt-Callbacks benötigen eine explizite Current-Context-Guard. Cleanup muss sowohl auf `aboutToBeDestroyed` als auch im Destruktor sicher funktionieren und erneute Initialisierung erlauben. Dies folgt dem Lebenszyklus in der [Qt-Dokumentation zu QOpenGLWidget](https://doc.qt.io/qt-6/qopenglwidget.html).

Gemeinsame Szene/Auswahl/Kamera/Eingabe werden einmal implementiert; nur die notwendige Backend-Anbindung unterscheidet sich. Windows-Native ist diagnostischer Fallback gemäß ADR, nicht eine zweite unabhängige Produktimplementierung. DPI/FBO/Resize/Reparent/Context-Loss müssen getestet werden. Natives Wayland wird durch tatsächlichen QPA-Status belegt; vorhandene X11-Bibliotheken allein beweisen weder XWayland-Nutzung noch deren Abwesenheit.

## K10 – Fachlicher v1-Vertrag

V1 liefert metrische Einzelteile und mehrere Körper in einem Dokument: Basisebenen/planare Stützflächen, Skizzen mit vorhandenen elementaren Kurven und Constraints, Regionen mit Löchern, Extrusion als NewBody/Join/Cut/Intersect, einfache Verrundung/Fase, Bohrung, Spiegelung/Muster, Edit/Suppress/Rollback, Messung und STEP-Austausch. Vollständige Montagezwänge, technische Zeichnung, freie NURBS-Modellierung, Blechabwicklung und CAM folgen als benannte G5-Module.

Extrusionsparameter trennen Profil, Zielkörper, Operation, Richtung, Start-/Endbedingung und Distanz. Zunächst Blind/Reverse/Symmetric mit konstanter Querschnittsform; weitere Endbedingungen erst nach eigenem Vertrag. Taper, Revolve, Shell und andere sichtbare Erweiterungen bleiben deaktiviert, bis ihr vollständiger Pfad und ihre Abnahme existieren; X19 liefert den nächsten Solid-Ausbauschritt. Der Produktrand ist in UI und Handbuch gleich beschrieben.

## K11 – Definition of Ready / Done

Ready: Paketvoraussetzungen fertig; relevante Verträge eindeutig; betroffene API bekannt; ein beobachtbarer Erfolgsfall und die entscheidenden Fehlerfälle beschrieben. Entscheidungspakete dürfen mit klar begrenzter offener Hypothese beginnen; Implementierungspakete nicht mit ungeklärter Datenautorität.

Done: Anwendungspfad funktioniert; Fehler hinterlassen den vereinbarten Zustand; passende Regression besteht; relevante Bestandsprüfungen und Vertragschecks bestehen; Migrations-/Plattformwirkung ist geprüft oder als noch blockierendes Gate markiert; keine Attrappen als umgesetzt dokumentiert. Testumbenennungen oder kleinere Dateien allein erfüllen keinen Fachauftrag.

Der ausführende Agent verantwortet diese Prüfungen und bei sichtbaren Änderungen die eigene optische Kontrolle nach [Kapitel 10](10-autonome-umsetzung.md). Ein routinemäßiges „bitte selbst ausprobieren“ erfüllt Done nicht. Bei beauftragter Gesamtumsetzung führt bestandene Paketabnahme ohne erneute Nutzeraufforderung zum nächsten zulässigen Paket; offene externe Gates bleiben dabei wirksam.
