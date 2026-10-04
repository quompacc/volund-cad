# ADR 0010 – Native Konstruktionsaktionen und nachrüstbare Analyse

- Status: accepted (Architektur-/Planungsentscheidung, keine implementierte KI/FEM)
- Nachtrag 04.10.2026: AI02 direkt nach R10b, AI03 direkt danach ohne L10-Bindung;
  Anbieter zunächst Claude per API, austauschbar (Kapitel 11).
- Datum: 17. September 2026
- Grundlage: Nutzerauftrag zu nativer KI-Konstruktion, FEM-Wiederverwendung und gestuftem Ausbau.

## Entscheidung

GUI, integrierter Assistent und ein optionaler MCP-Adapter verwenden dieselben
typisierten Application-Aktionen und Modellabfragen. Ein Skriptinterpreter ist keine
Voraussetzung für Konstruktion. Das Dokument bleibt alleinige Modellautorität.
Keine KI- oder Solverlogik im Dokument, keine direkte Mutation durch externe Adapter.

Jetzt verbindlich sind explizite Ziel-IDs, Einheiten, Dokument-/Sitzungsstand,
aktuelle Auswertungszustände und Transaktionsgrenzen. Vor Zusammenführung des
Ribbon-Routings liefert AI02 diese gemeinsame Grenze für den vorhandenen Umfang.
Bestehende synchrone CadSession-Aufrufe werden schrittweise adaptiert, nicht pauschal ersetzt.
R05/R06 und R09/R10 bleiben notwendige Referenz-/Regionsarbeit; KI umgeht sie nicht.

Der integrierte native Assistent ist die spätere primäre KI-Bedienung. MCP ist ein
zusätzlicher Zugang und weder Voraussetzung noch Ersatz für diesen Produktworkflow.
Modell-/Anbieteradapter bleiben austauschbar; die Fachbefehle hängen von keinem SDK ab.
Ein Modell darf nur verfügbare Fähigkeiten aufrufen. Freitext ist kein ausführbarer Code.

FEM wird später als eigener Arbeitsbereich mit Analysefall, Material, Lasten,
Lagerungen, Netz und revisionsgebundenen Ergebnissen angebunden. Ein Analyseauftrag
besitzt eine unveränderliche Eingabe einschließlich Geometrie-, Referenz-, Material-,
Randbedingungs-, Netz- und Solverstand. Ergebnisse verändern keine CAD-Parameter.
Änderungen machen alte Ergebnisse sichtbar veraltet; Netznummern ersetzen keine CAD-IDs.
Simulation, Parameteroptimierung, Topologieoptimierung und Geometrierückführung sind
getrennte Lieferungen. Eine eigene Modelltrainingspipeline ist keine Voraussetzung.

## Wiederverwendung

FEM01 untersucht CalculiX und Gmsh als Kandidaten, keine bereits zugelassenen Abhängigkeiten.
FreeCADs FEM dient als Referenz für Formate, Einheiten, Zuordnung und Prüffälle.
Abtrennbarer Quellcode darf mit Commit, Lizenz, Abhängigkeiten und Herkunftsnachweis
portiert werden. Eine Übersetzung nach C++ bleibt eine Ableitung. Python und die
FreeCAD-Runtime bleiben gemäß Architekturvertrag ausgeschlossen.
Solver/Mesher zunächst als isolierte Prozesse evaluieren; Bibliotheksanbindung nur
nach begründetem Vergleich. Keine Qt-/OCCT-Zeiger über Prozess- oder Modellgrenzen.
Die konkreten Lizenz-, Distributions- und Windows-/FreeBSD-Nachweise gehören in FEM01.

## Alternativen und Grenzen

- Vollständige FreeCAD-FEM-Portierung: zunächst verworfen wegen Kopplung an
  Dokumentmodell, Python und GUI; gezielte Wiederverwendung bleibt erwünscht.
- MCP mit beliebiger Skriptausführung als Produktkern: verworfen; fachliche Aktionen
  müssen selbst dokumentiert, testbar und unabhängig vom Transport sein.
- Alle Analyseattribute und ein generisches Jobframework jetzt einbauen: verworfen,
  da noch kein konkreter FEM-Vertrag vorliegt. Keine leeren Persistenzfelder reservieren.
- Eigene neuronale Formgenerierung zuerst: zurückgestellt; numerische Optimierung
  liefert den ersten nachvollziehbaren Weg zu belastungsabhängiger Formfindung.

## Folgen und Gegenprüfung

AI01 beschließt nur den Vertrag. AI02 bleibt Implementierungsarbeit vor U04.
KI-UI und alle numerischen Erweiterungen warten auf eigene Liefer-/Abnahmenachweise;
G4 verlangt weder Modellzugang noch FEM-Installation. Ausbaureihenfolge und Szenarien:
[Kapitel 11](../implementation-plan/11-ki-fem-optimierung.md).

Gegenprüfung: Die Grenze erhält die bestehende Dokumentautorität, erzwingt keinen
zweiten Undo-Stack und vermischt weder Revisionsnummern verschiedener Sitzungen noch
CAD-/Netzidentitäten. Mehrteilige Aktionen brauchen isolierte Kandidaten und genau
einen Commit; eine Folge bereits veröffentlichter Einzelbefehle ist kein atomarer Batch.
Keine zukünftige Fähigkeit wird durch diese Entscheidung als implementiert ausgewiesen.
