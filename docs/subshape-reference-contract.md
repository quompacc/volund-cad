# SubshapeRef und Auflösungsvertrag – R02

Status: angenommen als Entscheidung für R03–R06, 17.09.2026.
Der kompilierbare [C++-Entwurf](../tools/reference-contract/SubshapeContract.hpp)
und seine [Fixtures](../tools/reference-contract/ContractProof.cpp) sind isoliert.
Der produktive Resolver und das native Anwendungsschema 1 bleiben unverändert.
Diese Entscheidung konkretisiert K06 und den Befund F06; sie garantiert kein
universelles Topological Naming. Verantwortlich: Völund-Dokument-/Feature-Schicht.

## Identität und Schichtgrenzen

`document::SubshapeRef` soll einen Dokumentnamensraum, BodyId, originFeature,
erwartete Topologieart und einen typisierten semantischen Herkunftsschlüssel
enthalten. `originFeature` bezeichnet den ursprünglichen Erzeuger, nicht den
aktuellen Modifier. Beide Rollen dürfen nicht zusammengelegt werden. Die
Zielauswertung wird separat durch Dokument-ID, BodyId, outputFeature und Revision
bestimmt. Ein Shape-Handle oder Kandidat aus einer anderen Revision ist ungültig.
Im Prototyp liegen diese Typen im Namespace `reference_contract`.

| Schlüsselvariante | Beispiel | Identitätsregel |
| --- | --- | --- |
| BoxFaceKey | Feature 1, `positiveZ` | Lokale Erzeugungsachse und Vorzeichen; Rotation ändert die Rolle nicht |
| ExtrusionCapKey | Feature 3, Region `outer-loop-A`, `end` | Semantischer Start/Ende entlang der Extrusionsrichtung, nicht Welt-Z |
| ContourEdgeKey | Sketch 2, Element 17, `rectangleBottom` | Element-ID plus lokale Konstruktionsrolle; kein Explorer-/Segmentindex |
| ExtrusionSideKey | Feature 3, Region `outer-loop-A`, vorgenannte Konturkante | Mantelfläche aus genau dieser Grenze |

`wholeCurve` bezeichnet Linie/Kreis/Bogen/Ellipse als analytische Kurve;
Rechteckseiten haben feste Rollen in der Erzeugungsparametrisierung. Bei
unzulässiger/degenerierter Geometrie entsteht kein gültiges Naming-Ergebnis.
Eine Umkehrung von Kurven- oder Extrusionsrichtung muss explizit abgebildet werden.
Topologieart passt zur Schlüsselvariante; IDs sind nicht null, Regiontokens nicht
leer, Enumwerte und fremde Namensräume werden an Eingabegrenzen validiert.
Neue Rollen erfordern Schema- und Regressionserweiterung.

Die Regiontokens der Fixtures sind deklarierte Symbolnamen. Der heutige
SketchProfileId ist **keine** dauerhaft stabile Regionsidentität. R09 muss die
Identität aus stabilen Konturgrenzen umsetzen; bis dahin dürfen R03–R06 für
Extrusionsflächen nur konkret belegte unveränderte Grenzen fortführen und müssen
bei unklarer Regionszuordnung einen Reparaturzustand liefern. R04 kann unabhängig
davon bekannte Primitive und deren belegte Historie anbinden.

`document` speichert Referenzen und Reparaturentscheidungen. `kernel` liefert
Operationshistorie mit Lebensdauerbesitz der Shapes. `features` verfolgt Herkunft
und veröffentlicht das revisionsgebundene Resultat. `application` besitzt den
Reparaturbefehl. Viewport/Qt konsumieren dieselben Kandidaten, ohne eigene
Normalen-/Nähe-Auflösung. TDF_Label, TNaming-Handle und TopExp-Reihenfolge sind
keine öffentliche Identität. Umbenennung und Save-As erhalten Referenzen.
Explizites Dokumentduplikat muss interne Dokument-IDs geschlossen umschreiben;
fremde Dokumentreferenzen werden nicht automatisch lokal gemacht.

## Resultate und Historienregeln

Ein Resultat enthält Status, stabile Reason, Zielscope und Kandidaten. Jeder
Kandidat besitzt einen nur in dieser Auswertung gültigen Shape-Token sowie im
Produkt den zugehörigen besitzenden Shape-Handle und die Herkunftsbelege.
Kandidatentokens werden niemals als dauerhafte Referenz gespeichert.

| Beleglage | Status | Verhalten |
| --- | --- | --- |
| Vollständige Herkunft, genau ein kompatibler Nachfolger | Resolved / unique | Aktuelle Geometrie verwenden |
| Vollständige Herkunft, mehrere unterschiedliche Nachfolger | Ambiguous / split | Alle Kandidaten anbieten; keine Auswahl nach Größe, Abstand oder Reihenfolge |
| Explizit vollständig gelöscht | Missing / deleted | Kein geometrischer Ersatz |
| Historie fehlt oder ist unvollständig, null/ein Vorschlag | Missing / historyUnavailable | Auch ein einzelner Vorschlag bleibt unbestätigt |
| Historie fehlt, mehrere plausible Vorschläge | Ambiguous / historyUnavailable | Migration/Reparatur benötigt bewusste Zuordnung |
| Falscher Dokument-/Körper-/Auswertungsstand | Missing / wrongScope | Keine Kandidaten aus fremdem Stand veröffentlichen |
| Anderer Herkunftsschlüssel/Erzeuger | Missing / unknownOrigin | Kein Rollenwechsel |

Vollständige, leere Historie ist ohne explizite Löschungsinformation ein
Vertragsfehler. Widersprüchliche Löschung mit Nachfolgern oder falsche Topologie
sind ebenfalls keine gültige Geometrieauflösung. Infrastrukturfehler bleiben
Exceptions; der spätere Adapter unterscheidet sie von erwarteten Missing-Fällen.

R03 muss unveränderte, modifizierte, erzeugte und gelöschte Geometrie pro Operation
unterscheiden. `Generated` allein belegt keine Fortsetzung: eine neue Schnittwand
ersetzt nicht automatisch die gelöschte Boxdeckfläche. Ein expliziter
Operationsadapter darf erzeugte Shapes nur mit definierter semantischer Rolle
zuordnen. Leere `Modified`-Listen bedeuten weder automatisch unverändert noch
gelöscht. Identitätsfortsetzung verlangt einen belegten unveränderten Shape oder
eine vollständige Historienkette. Geometrievergleich allein genügt nicht.

Bei Split werden alle belegten Nachfolger verfolgt. Beim Merge darf eine alte
Referenz auf den gemeinsamen einzigen Nachfolger auflösen; mehrere alte
Referenzen können damit dieselbe Fläche bezeichnen. Split gefolgt von Merge
ergibt Resolved, wenn alle Pfade vollständig sind und auf dieselbe aktuelle
Fläche führen. Dedupliziert wird nach Shape-Identität inklusive Placement, nicht
nach gleicher Geometrie. Sortierung dient nur reproduzierbarer Kandidatenanzeige.
Eine unvollständige Teilkette verhindert Resolved auch dann, wenn ein anderer
Pfad genau einen Nachfolger liefert. Ein Körperwechsel wird nicht implizit
verfolgt; künftige Körper-Merges brauchen einen eigenen Herkunftsvertrag.

Rotation/Translation erhalten den Schlüssel. R04 übernimmt Ebene und Basis aus
der aktuellen referenzierten Fläche samt nachgewiesener Transformation; die alte
Normale ist kein Identitätsfilter. Die alte Ebene bleibt höchstens als ausdrücklich
veraltete Reparaturvorschau. Flächenorientierung und rechtshändige Skizzenbasis
werden gegen den D10-Ebenenvertrag geprüft.

Projizierte Punkte sollen neben der Kantenreferenz eine Auswahl `start`, `end`
oder einen normierten Parameter auf der orientierten analytischen Quellkurve
speichern. Kreisnaht, Richtung und Parameterabbildung gehören zur Provenienz.
Eine fehlende Abbildung nach Split/Merge wird reparaturbedürftig; kein Clamping
auf einen zufälligen Teilbogen. R05 muss die konkrete Kurvenabbildung und
Tessellierungsunabhängigkeit beweisen. Der Prototyp modelliert nur die Kantenidentität.

## Konkrete Referenzfixtures

Längen in mm. Ausgangsbox A: [0,10]×[0,10]×[0,10], Referenz positiveZ.
Koplanare Box B: [20,30]×[0,10]×[0,10]. Beide Deckflächen haben +Z und z=10;
der alte Normalen-/Distanzresolver kann sie nicht unterscheiden. Die Tabelle
definiert die **Sollhistorie**, die R03 mit echtem OCCT erzeugen/prüfen muss.
Die C++-Fixtures prüfen deren Klassifikation und umgekehrte Kandidatenreihenfolge.

| Fixture-ID | Operation bzw. Eingabe | Erwartung |
| --- | --- | --- |
| box-top | A unverändert, belegte Deckfläche A | Resolved |
| coplanar-known-origin | A/B koplanar, nur A stammt vom Schlüssel | Resolved A |
| coplanar-legacy-tie | Nur alte Ebene z=10, Vorschläge A/B | Ambiguous, keine Zufallsauswahl |
| split | Nut x=[4,6], y=[-1,11], z=[9,11] schneidet A-Deckfläche in zwei Teile | Ambiguous A/B |
| merge | Zwei benachbarte Deckflächen werden nach Union/Unify zu einer belegten Fläche | Resolved; falls Kernel zwei erhält: Ambiguous |
| split-then-merge | Beide Splitpfade führen vollständig zu derselben aktuellen Fläche | Resolved, einmal dedupliziert |
| deleted | Cut x/y=[-1,11], z=[9,11] entfernt die ursprüngliche Deckfläche | Missing; neue Schnittfläche z=9 ist kein Ersatz |
| rotated-90 | A um 90° um Welt-Y gedreht, lokale +Z-Fläche jetzt +X | Resolved; R04 muss Skizzenbasis mitdrehen |
| boolean-modified | Seitlicher Cut x=[8,11], y/z=[-1,11], Deckfläche bleibt ein Stück | Resolved bei belegtem Modified |
| boolean-generated-is-not-continuation | Neue Schnittwand, ursprüngliche Fläche gelöscht | Missing |
| boolean-history-gap | Ein Historienglied fehlt | Missing |
| single-geometric-suggestion | Nur ein geometrischer Kandidat ohne Herkunft | Missing mit Vorschlag |
| foreign-document / foreign-body | Gleiche lokalen Rollen, falscher Namensraum/Körper | Missing |
| stale-revision / wrong-output-producer | Historie stammt aus r41 oder anderem Produzenten statt r42/Feature 9 | Missing |
| different-origin-role / different-origin-feature | Gleiche Geometrie, anderer Schlüssel | Missing |
| extrusion-end-cap | Rechteckregion A, Extrusion 3, semantisches Ende | Resolved mit vollständiger Herkunft |
| contour-edge-tessellation-independent | Sketch 2 / Element 17 / rectangleBottom, beliebige Anzeigesegmente | Gleiche Kantenidentität |
| extrusion-side | Mantelfläche genau dieser Regionsgrenze | Resolved mit vollständiger Herkunft |
| Fünf ungültige Eingaben | Falsche Topologie, widersprüchliche Löschung, unbegründete leere Historie, Null-ID, leere Region | Vertragsverletzung abweisen |

Die Merge-Regel verlangt keine bestimmte Kernel-Vereinfachung. Entscheidend ist
die tatsächlich belegte Anzahl verschiedener Nachfolger. Diese Fixtures behaupten
keine schon gemessene OCCT-Historie und keine bereits bestandene Skizzenrotation.

## Persistenz und Migration für R06

Native BinOcaf-Schreibversion 12 bleibt bestehen. Die Erweiterung benötigt eine
neue explizite **Anwendungsschemaversion**, die erst bei Integration vergeben und
auf beiden OCCT-Versionen geprüft wird. R02 ändert weder Schema 1 noch VOLUND 1–16.
Die bestehenden Schema-1-Dateien sind ebenfalls Migrationsquellen.

Geplante OCAF-Inhalte pro Referenz: Referenzart/Schlüsselversion, DocumentId,
BodyId, Erzeugerreferenz, Topologie, typisierte Rollenfelder und erforderliche
Region-/Element-/Grenzreferenzen. Interne TDF_References sind Hilfsverbindungen;
gespeicherte IDs müssen nach Laden übereinstimmen. Labeltags werden bei R06 gegen
die dann bestehende native Struktur vergeben, nicht hier blind reserviert.
Unaufgelöste Referenzen behalten Originalschlüssel bzw. Legacyfelder plus
Migrationsgrund und optional letzte Ebene. Kandidaten und Revisionsergebnisse
werden neu berechnet; sie sind keine persistente Geometrieautorität.

Legacy-Segmentnummern dürfen nur gegen exakt rekonstruierte Ausgangsgeometrie
interpretiert werden. R06 darf automatisch migrieren, wenn eindeutige semantische
Erzeugung und vollständige Herkunft belegt sind. Normalen-/Abstands-/Signaturtreffer
werden als Vorschlag klassifiziert, auch bei nur einem Treffer. Zweifelhafte
Fälle bleiben `MigrationNeedsRepair` mit originalen Daten. Open schreibt die
Quelldatei nicht; erst Save nutzt den bestehenden atomaren Schreibpfad.
Unbekannte Schlüsselversionen/Enumwerte werden abgewiesen, beschädigte Referenzen
werden nicht still ergänzt. Save/Open muss auch reparaturbedürftige Dokumente
verlustfrei erhalten. Native Metadaten-/Strukturprüfer und Snapshotvalidierung
werden gemeinsam erweitert; kein zweiter unabhängiger Referenzspeicher.

## Reparaturbefehl – Entwurf für R06

`RepairSubshapeReference` erhält erwartete DocumentId/Revision, betroffenes Feature,
Referenzslot (Support oder Constraint-ID), alten Schlüssel und bewusst gewählten
Kandidaten samt neuem semantischem Schlüssel. Keine Parameteränderung im Widget.
Vor Commit prüft application aktuellen Stand, Slotinhalt, Körper/Topologie,
vollständige neue Herkunft und DAG-Zyklen. Ein veralteter Kandidat wird abgewiesen.
Der neue Schlüssel kann einen anderen Erzeuger haben; bloßes Speichern eines
flüchtigen Shape-Tokens reicht nicht.

Ein Kandidat wird über die bestehende CommandTransaction ausgewertet. Referenz,
Skizzenbasis/abgeleitete Projektionen und R01-Fehlerstatus werden in genau einer
Transaktion veröffentlicht. Cancel, Exception oder abgewiesene Reparatur erhalten
Referenz, Dirty, Auswahl und Undo-Stack. Folgefehler verlangen die vorhandene
explizite `keepEditableErrors`-Entscheidung und `CommittedWithErrors`. Undo stellt
den alten reparaturbedürftigen Stand wieder her; Redo den neuen. Save/Open erhält
die Zuordnung ohne flüchtige Kandidaten. Diese Produktabläufe bleiben R06-Abnahme.

## Alternativen und Grenzen

Verworfen: nächster Normalen-/Abstandstreffer, größtes Splitstück, erster Explorer-
Index, Shape-Hash als dauerhafte Identität und alleinige TNaming-Labelnummer.
Alle können nach Parameteränderung unbeabsichtigt andere Geometrie bezeichnen.
Geometriesignaturen bleiben Plausibilitätsprüfung und explizite Reparaturhilfe.
Eine unplausible eindeutig benannte Fläche wird diagnostiziert, nicht durch
eine ähnliche ersetzt. Unsupported Operation History bleibt sichtbar ungelöst.

Die Regeln werden erweitert, wenn R03 einen tatsächlichen Kernel-Historienfall
nicht eindeutig abbilden kann, R09 Regionsidentität bindet oder neue Operationen
eigene Rollen benötigen. Jede Erweiterung erhält Gegenfixtures und ein geprüftes
Migrationsverhalten. R03–R06, R09 und A-REF-01/02/03 sind durch R02 nicht erledigt.

## Ausdrücklich gestartete Sammelreparatur (R06d)

Nutzerentscheidung vom 18.09.2026: Der Auto-Klick autorisiert die unmittelbare
Übernahme eindeutig zur alten Ebene passender Supportflächen mit aktueller
Herkunft. Die lesende Kandidatenanalyse und Open mutieren weiterhin nichts.
Geometrische Eindeutigkeit beweist keine historische Identität; Auto ist eine
bewusste Ersatzzuordnung. Mehrdeutige Kandidaten, alte Kantenattachments und
fehlende Herkunft verbleiben in der Reparaturliste. Batch-Prüfung und Anwendung
folgen der Abhängigkeitsordnung und bilden eine atomare Undo/Redo-Transaktion.
