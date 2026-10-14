# 07 – Ausbau zur gesamten Produktfamilie

Der erste CAD-Kern ist nicht die vollständige Vision. Dieses Kapitel hält die weiteren Arbeitsbereiche ausdrücklich fest. Ihre erste vertikale Lieferstufe ist benannt; wo Fachentscheidungen oder fremde Schnittstellen fehlen, ist das nächste ausführbare Paket die begrenzte Spezifikation samt API und Tests. Es wäre irreführend, heute jeden späteren Montage- oder CAM-Algorithmus als bereits entschiedenen Codingauftrag auszugeben.

Die bei neuen Modulen genannten existierenden Dateien sind Integrations- und Lektüreeinstiege. Neue Fachlogik gehört in das jeweilige Modul und passende Kernschicht, nicht pauschal in die genannten heute zentralen Dateien. Überschreitet eine vertikale Lieferung die Paketgrenze, erstellt das vorausgehende Spezifikationspaket deren verbindliche Unter-IDs (Datenmodell, Evaluator, Store, Controller, Abnahme). Diese müssen vor Implementierung Ready erfüllen.

## Liefergrenzen und externe Voraussetzungen

| Bereich | Erste Lieferung | Noch zu spezifizieren / extern benötigt |
|---|---|---|
| Assembly | Instanzen, Platzierung, begrenzte Mates | Breiter 3D-Constraintumfang, Kinematik, große Baugruppen |
| Drawing | Ein Blatt, Projektionen, einfache assoziative Maße | Normumfang, GD&T, Schnitt-/Detailansichten, Stücklisten |
| Surface | Parametrischer Loft mit klaren Resultattypen | Freiformedit, Patch/Sweep, erweiterte Kontinuität |
| Mesh | Import, Inspektion, begrenzte Reparatur | Remeshing, sichere Rekonstruktion, große Scans |
| Sheet Metal | Basislasche, eine Biegung, Abwicklung | Material-/Prozesskalibrierung, Mehrfachbiegung/Ecken |
| Vault | Lokale Revisionen, dann bestätigter Syncadapter | Reale API/Authentifizierung, Berechtigungen, Serverkonflikte |
| Manufacturing | Revisionsgebundene Artefaktübergabe | Tatsächliches nachgelagertes Prozesssystem |
| CAM | Ein festgelegter 2.5D-Prozess bis Simulation | Maschinenprofil, Werkzeuge, Schnittdaten und reale Freigabe |
| Weitere Solids | Eigene Pakete je Revolve/Shell/Draft/Sweep usw. | Konkrete Feature-/Fehlermatrix statt Ribbonversprechen |
| Native KI | Auswahlbezogene Aktionsvorschläge und bearbeitbare Features | Gemeinsame CAD-Aktionen früh in AI02; Modelladapter/UI später AI03 |
| FEM | Lineare Statik eines Volumenkörpers | Komponenten-/Lizenz-/Plattformprüfung, Vernetzung und numerische Abnahme FEM01–03 |
| Optimierung | Zunächst bestehende Featureparameter | Organische Topologie und CAD-Rückführung getrennt in OPT02–04 |

**Prüfweg:** jedes Modul erbt K01–K11, die sichere Dokumentbasis und den Plattform-/Releaseprozess. Ein neues Modul braucht kein neues allgemeines Appframework; WorkspaceRegistry und Toolhost werden erweitert. Für fremde Services sind deren tatsächliche Dokumentation und Autorisierung erforderlich.

K12/K13 gelten zusätzlich. [Kapitel 11](11-ki-fem-optimierung.md) enthält AI01–AI04,
FEM01–FEM03 und OPT01–OPT04. Die dortigen G5-Zweige warten auf ihre eigenen
Voraussetzungen, nicht auf X20 oder die gesamte übrige Produktfamilie. Bestehende
Solver/Mesher und gezielt portierbare FreeCAD-Bausteine werden bevorzugt geprüft;
eine komplette FreeCAD-/Python-Runtime bleibt ausgeschlossen.

## X01 – Baugruppenvertrag und Referenzbaugruppe spezifizieren

- **Voraussetzungen:** L10.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `include/voelund/document/FeatureId.hpp`, `src/workspaces/CommandRegistry.cpp`, `docs/architecture-and-code-contract.md`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. AssemblyDocument, PartDocument, ComponentInstance und Placement unterscheiden; Versionierung, externe Referenzauflösung und Einbetten/Verknüpfen entscheiden.
  2. Referenzbaugruppe aus Grundplatte, zwei identischen Bolzen und einer beweglichen Lasche mit Sollpositionen festlegen.
  3. Import von STEP-Assemblystruktur gegen OCCT-XDE prüfen und Arbeitsaufträge für Datenmodell, Resolver, Darstellung und Store ausformulieren.
- **Abnahme:** Entwurf besitzt konkrete Header, Dateischema, Einheiten-/Transformregeln, Missing-Part-Verhalten und native Fixture. Jede folgende Implementation hat höchstens eine Verantwortlichkeit.
- **Grenze:** Kein Aufweichen der Body-/Feature-ID-Semantik, um Instanzen wie neue Körper zu behandeln.

## X02 – Baugruppeninstanzen und Platzierungen liefern

- **Voraussetzungen:** X01.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `src/document/ModelDocument.cpp`, `src/features/FeatureGraph.cpp`, `src/viewport/CadViewportQtOpenGl.cpp`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Bestätigten Assembly-Snapshot/Store mit Instanz-ID, Teilreferenz und Transformation implementieren.
  2. Mehrere Instanzen desselben Teils ohne duplizierte Parameterwahrheit darstellen und auswählen.
  3. Fehlende Teilrevision sichtbar behandeln; Relink als Transaktion. Gemeinsame Teiländerung aktualisiert abhängige Instanzen.
- **Abnahme:** Referenzbaugruppe hat zwei unterscheidbare Bolzeninstanzen desselben Teils. Save/Open, Teilverschiebung, Quelle umbenennen und Missing-Part-Reparatur geprüft.
- **Grenze:** Noch keine allgemeine Montage-Zwangslösung.

## X03 – Erste Montagebeziehungen mit DOF-Diagnose

- **Voraussetzungen:** X02.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `src/features/FeatureGraph.cpp`, `src/modules/solid/SolidWorkspaceModule.cpp`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Eigenes Assembly-Modul mit Fixed und klar begrenztem Coaxial/Planar-Mate-Vertrag entwerfen; Solverwahl durch ADR prüfen, vorhandenen 2D-PlaneGCS nicht ungeprüft als 3D-Lösung verwenden.
  2. Mindestens Fixed plus eine nachgewiesene bewegliche Beziehung implementieren, DOF/Redundanz/Konflikt anzeigen.
  3. Constraint-Edit/Undo/Persistenz und unabhängige Instanzidentität erhalten.
- **Abnahme:** Lasche auf Bolzen besitzt erwartete verbleibende Beweglichkeit; widersprüchliches Mate erzeugt verständlichen Konflikt ohne Positionssprung anderer Teile. Fixture samt numerischen Grenzen dokumentiert.
- **Grenze:** Volle kinematische Simulation und beliebige 3D-Constraints sind weitere Spezifikationspakete.

## X04 – Erste assoziative technische Zeichnung liefern

- **Voraussetzungen:** X01, L10.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `src/kernel/CMakeLists.txt`, `src/workspaces/CommandRegistry.cpp`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Drawing-Modul mit Blatt, Maßstab, Basisansicht, Projektion und referenzierten Modellkanten spezifizieren; zulässige OCCT-HLR-/Vektorexportwege prüfen.
  2. Zunächst ein Blatt mit Vorder-/Seitenansicht, einfacher assoziativer Längenbemaßung und Titelblock implementieren.
  3. PDF-/Vektorausgabe, Schrift-/Einheitenregeln und gebrochene Maßreferenzen explizit testen; dafür neue Abhängigkeit nur nach ADR.
- **Abnahme:** Modellbreite ändern aktualisiert Ansicht und Maß. Fehlende Kante ergibt kaputte Referenz statt falsches Maß. Maßstab verändert Papierdarstellung, nicht Zahlenwert. Ausdruck/Export visuell und metrisch geprüft.
- **Grenze:** GD&T, Stücklisten, Schnitt-/Detailansichten und komplette Zeichnungsnormabdeckung brauchen separate Folgepakete.

## X05 – Surface-Daten- und Genauigkeitsvertrag festlegen

- **Voraussetzungen:** L10.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `src/kernel/ShapeFactory.cpp`, `src/features/FeatureGraph.cpp`, `src/workspaces/CommandRegistry.cpp`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Kurven/Flächen/Shells/Solids als unterschiedliche Resultattypen und zulässige Übergänge festlegen.
  2. Ersten Scope wählen: Loft zwischen zwei kompatiblen Profilen, Randreferenzen, Kontinuitätsanforderung und Sew-Solid-Bedingungen.
  3. Analytische und geometrisch problematische Fixturematrix plus konkrete Kernel-/Feature-/UI-/Store-Unterpakete erstellen.
- **Abnahme:** Entwurf erklärt offene Fläche, geschlossene Hülle und validen Solid; Toleranzen und G0/G1-Kontinuität sind prüfbar. Keine freie NURBS-Komplettheit suggeriert.
- **Grenze:** Spec-Paket; keine zusätzliche Geometryengine.

## X06 – Ersten parametrischen Surface-Loft vertikal liefern

- **Voraussetzungen:** X05.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `src/kernel/ShapeOperations.cpp`, `src/features/FeatureGraph.cpp`, `src/desktop/ToolPanelHost.cpp`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Loft-Feature nach X05 mit stabilen Profilreferenzen evaluieren und darstellen.
  2. Verdrehung/Orientierung, inkompatible Profile und Selbstschnitt explizit behandeln.
  3. Preview/Edit/Undo/Save/Open und STEP-Export im Surface-Workspace anschließen.
- **Abnahme:** Zwei definierte Profile erzeugen erwartete offene bzw. geschlossene Form; Quellmaßänderung aktualisiert sie. Fehlerfälle lassen bisheriges Modell erhalten; Resultattyp korrekt ausgezeichnet.
- **Grenze:** Patch, Sweep, freie Kontrollpunktbearbeitung und komplexe Kontinuität erst nach gesonderten Tickets.

## X07 – Mesh-Modell und Import-/Exportgrenzen spezifizieren

- **Voraussetzungen:** L10.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `src/kernel/CMakeLists.txt`, `src/workspaces/CommandRegistry.cpp`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. TriangleMesh getrennt von exaktem BRep halten; Einheiten, Normalen, Indizes und Ressourcenlimits definieren.
  2. Ersten Scope STL/gegebenenfalls vorhandenes geeignetes Format, Anzeige, Statistik und einfache Reparatur festlegen; keine neue Library ohne ADR.
  3. Fixtures für geschlossene Schale, Loch, Nonmanifold-Kante, invertierte Normale und riesige Eingabe vorbereiten.
- **Abnahme:** Konkrete API und Dateiformatentscheidungen; Genauigkeitsverlust gegenüber CAD sichtbar benannt. Importlimits vor großen Allokationen prüfbar.
- **Grenze:** Meshimport ist keine automatische Rekonstruktion parametrischer CAD-Geometrie.

## X08 – Mesh-Inspektion und begrenzte Reparatur liefern

- **Voraussetzungen:** X07.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `src/viewport/CadViewportQtOpenGl.cpp`, `src/desktop/ToolPanelHost.cpp`, `src/workspaces/CommandRegistry.cpp`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Mesh-Modul mit Import/Anzeige, Manifold-/Orientierungsdiagnose und Export implementieren.
  2. Nur nachgewiesene Reparaturen anbieten, zunächst Normalenumkehr und Entfernen degenerierter Dreiecke; Topologieänderung als Vorschau zeigen.
  3. Transaktion/Undo und eingebettete Meshquelldaten im Dokument erhalten.
- **Abnahme:** Fixtures melden richtige Defekte; Repair verändert nur vorgesehene Dreiecke und ist rückgängig. Keine 'repariert'-Meldung bei verbleibendem Nonmanifold-Fall.
- **Grenze:** Automatisches Löcherschließen/Remeshing erst nach Fehler-/Genauigkeitsvertrag.

## X09 – Blechvertrag mit Biegeparametern und Abwicklungsfixture

- **Voraussetzungen:** L10.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `src/features/FeatureGraph.cpp`, `src/workspaces/CommandRegistry.cpp`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Konstante Blechdicke, Innenradius, Winkel, K-Faktor/Biegezuschlag und Reliefs als explizite Parameter spezifizieren.
  2. Ersten Scope Basislasche plus eine gerade Biegung festlegen; Formteil/Abwicklung als zusammengehörige Ergebnisse modellieren.
  3. Analytische Fixture mit festgelegter Neutralfaserformel und Herkunft der Parameter erstellen; Anschluss an Zeichnung/Manufacturing planen.
- **Abnahme:** API und Formel erlauben eindeutigen Sollwert der Abwicklung. Ungültige Dicke/Radius/Kollision und nichtabwickelbare Geometrie sind definierte Fehler.
- **Grenze:** Keine universelle Fertigungsgenauigkeit ohne material-/prozessbezogene Kalibrierung behaupten.

## X10 – Basislasche, eine Biegung und Abwicklung liefern

- **Voraussetzungen:** X09.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `src/kernel/ShapeOperations.cpp`, `src/features/FeatureGraph.cpp`, `src/desktop/ToolPanelHost.cpp`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Parametrisches SheetMetal-Modul nach begrenztem X09-Scope implementieren; Dicke als Invariante prüfen.
  2. Gefaltete und abgewickelte Darstellung sowie Biegelinien als abgeleitete Ergebnisse anbieten.
  3. Maßänderung, Undo, Save/Open und geeignetes konturtreues Exportformat anbinden.
- **Abnahme:** Analytische Biegefixture stimmt innerhalb festgelegter Toleranz. Selbstüberschneidung/zu kleiner Radius nicht still akzeptiert. Fertigungsdaten benennen verwendete Biegeparameter.
- **Grenze:** Mehrfachbiegungen, Eckfreistellung und allgemeines Unfold brauchen anschließende kleine Pakete.

## X11 – Gemeinsame CAD/Vault/Fertigungsidentitäten konkretisieren

- **Voraussetzungen:** L10, D11.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `include/voelund/document/FeatureId.hpp`, `docs/architecture-and-code-contract.md`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. ProjectId, ModelId, immutable RevisionId und ArtifactId einschließlich Duplikat/Fork/Snapshot-Regeln spezifizieren.
  2. Gemeinsames Austauschmanifest mit Quellrevision, Schema, Einheiten, Prüfsumme, Artefakttyp und Provenienz definieren.
  3. Lokaler Dateipfad wird Locator, nicht Identität; Konflikt- und Referenzauflösung als API-Vertrag festhalten.
- **Abnahme:** CAD-Datei verschieben/umbenennen erhält Modellidentität; bewusste Kopie/Fork hat definierte neue Identität. Ein Artefakt lässt sich eindeutig zur Quellrevision zurückverfolgen.
- **Grenze:** Keine unbekannte Vault-API erfinden und kein Remotezugang voraussetzen.

## X12 – Lokale Projekt-/Revisionsverwaltung als erster Vault-Schritt

- **Voraussetzungen:** X11.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `src/application/CadSession.cpp`, `src/desktop/DocumentActions.cpp`, `src/workspaces/CommandRegistry.cpp`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Lokales Projektmanifest und unveränderliche gespeicherte Revisionen implementieren; aktuelle Arbeitskopie von Revision unterscheiden.
  2. Projekt öffnen, Revision benennen, ältere Revision ansehen und bewussten neuen Arbeitsstand ableiten.
  3. Artefakte am Manifest registrieren; Schreibvorgänge atomar und Mehrinstanzkonflikte sichtbar behandeln.
- **Abnahme:** Zwei Revisionen bleiben unverändert lesbar; Rückkehr zu alter Revision überschreibt neuere nicht. Abbruch während Manifestwrite beschädigt keine vorhandene Revision. IDs über Pfadänderung erhalten.
- **Grenze:** Noch kein Server, Benutzer-/Rollenmodell oder verdeckter Sync.

## X13 – Tatsächliche Vault-Anbindung spezifizieren

- **Voraussetzungen:** X12.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `docs/architecture-and-code-contract.md`, `src/platform/README.md`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Vorhandene echte Vault-Schnittstelle/Repositorydokumentation beschaffen und deren Authentifizierung, Versionierung, Upload, Locking und Fehlercodes inventarisieren.
  2. Adaptervertrag mit Offlinebetrieb, Conflict, Retry/Idempotenz und Credentialablage festlegen; fehlende externe Angaben als konkrete Fragenliste dokumentieren.
  3. Lokalen Fake-Server/Vertragstest für genau bestätigte API-Fälle vorbereiten.
- **Abnahme:** Reviewbare Schnittstellenmatrix und Fixtures existieren; keine ausgedachten Endpunkte. Ohne API-Unterlagen bleibt dieses Paket extern blockiert, lokale CAD-/Vault-Funktionen bleiben nutzbar.
- **Grenze:** Kein Kontakt zu externen Diensten und kein Versand von Projektdateien ohne passende Nutzerautorisierung.

## X14 – Minimalen Offline-/Online-Revisionsabgleich liefern

- **Voraussetzungen:** X13.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `src/application/CadSession.cpp`, `src/platform/README.md`, `src/desktop/DocumentActions.cpp`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Bestätigten Adapter für expliziten Push/Pull von Revision plus Artefakten implementieren; Credentials außerhalb Dokumentdateien halten.
  2. Timeout, Wiederholung, Teilupload, veraltete Basisrevision und Konflikt ohne stillen Datenverlust behandeln.
  3. UI zeigt lokale/remote Revision und ausstehende Aktion; Offlineedit bleibt eigenständig möglich.
- **Abnahme:** Vertragstests zeigen idempotente Wiederholung und Konflikt ohne Überschreiben konkurrierender Revision. Abgebrochener Transfer als unvollständig markiert, kein falsches synced.
- **Grenze:** Kein automatischer geometrischer Merge; Konfliktauflösung als bewusste Revision/Fork.

## X15 – Fertigungsübergabe als versioniertes Artefakt definieren

- **Voraussetzungen:** X11, M18.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `src/application/CadSession.cpp`, `src/desktop/DocumentActions.cpp`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. ManufacturingJob referenziert unveränderliche Modellrevision, ausgewählte Körper, Material-/Prozessmetadaten und Exportparameter.
  2. STEP/Mesh/Zeichnungs-/Abwicklungsartefakte mit Einheiten, Prüfsummen und Erzeugerversion manifestieren.
  3. Abgeleitete Artefakte bei neuer Modellrevision als veraltet kennzeichnen; keine unbemerkte Weiterverwendung.
- **Abnahme:** Änderung am CAD-Modell überschreibt freigegebenes Fertigungsartefakt nicht. Manifest kann Quelle und Parameter vollständig erklären. Fehlender/ungültiger Body verhindert erfolgreiche Übergabe.
- **Grenze:** Keine Maschinensteuerung und keine CAM-Bahnerzeugung in diesem Paket.

## X16 – Ersten Fertigungsworkflow integrieren

- **Voraussetzungen:** X15, X12.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `src/workspaces/CommandRegistry.cpp`, `src/desktop/ToolPanelHost.cpp`, `src/desktop/DocumentActions.cpp`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Einen konkreten Workflow auswählen und spezifizieren, zunächst revisionsgebundene STEP-Übergabe eines gefrästen Einzelteils.
  2. Export, Manifest, lokale Übergabe und Statusanzeige als eine nachvollziehbare Benutzeraktion anbinden.
  3. Fehlende Materialdaten, defekter Export und veraltete Revision korrigierbar behandeln.
- **Abnahme:** Referenzteil wird mit korrekter Revision/Einheiten exportiert; nach Modelländerung Status stale, bewusster Neuauftrag erzeugt neue Artefaktidentität. Kein Erfolg bei halb geschriebenem Export.
- **Grenze:** Externer Auftragversand nur im tatsächlich autorisierten Prozess, nicht automatisch durch Export.

## X17 – CAM-Scope, Maschinenmodell und Simulationsabnahme festlegen

- **Voraussetzungen:** X16.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `docs/architecture-and-code-contract.md`, `docs/dependencies.md`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Ersten Scope 2.5D mit festgelegtem Rohteil, Werkzeug, Koordinatensystem, Operation und Maschinenprofil spezifizieren.
  2. Strategie-/Geometrie-/Postprozessorgrenzen sowie Quellen für Schnittdaten und Maschinengrenzen festhalten; notwendige Libraries anhand ADR prüfen.
  3. Referenzpfad, Kollisions-/Restmaterialchecks und unabhängige Simulation als Abnahmekette planen.
- **Abnahme:** Kein offener Maschinen-/Werkzeugparameter wird geraten. Kleine Implementierungspakete für Setup, eine Bahnstrategie, Simulation und genau einen Postprozessor sind konkret ausgeschrieben.
- **Grenze:** Spezifikationspaket; keine pauschale Aussage, erzeugter G-Code sei für beliebige Maschinen ausführbar.

## X18 – Ersten CAM-Prozess bis zur geprüften Simulation liefern

- **Voraussetzungen:** X17.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `src/workspaces/CommandRegistry.cpp`, `src/desktop/ToolPanelHost.cpp`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Bestätigten 2.5D-Prozess in getrennten Kern-/Feature-/UI-Bausteinen implementieren.
  2. Setup/Toolpath als revisionsgebundene Daten speichern; Simulation und Postprozessor gegen das festgelegte Profil prüfen.
  3. Kollision, unerreichbare Geometrie und veraltete Quellrevision als blockierende Jobzustände behandeln.
- **Abnahme:** Referenzwerkstück besitzt reproduzierbaren Bahnverlauf und bestandene festgelegte Simulation. Postprozessoroutput entspricht vereinbartem Maschinenprofil; echte Maschinenfreigabe bleibt gesonderte reale Abnahme.
- **Grenze:** Keine direkte Maschinenbewegung als automatischer Softwaretest.

## X19 – Weiteren Solid-/Parameterumfang in nächste Lieferpakete zerlegen

- **Voraussetzungen:** L10.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `src/modules/solid/SolidWorkspaceModule.cpp`, `src/features/FeatureGraph.cpp`, `include/voelund/document/ModelDocument.hpp`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Nächste konkrete Features jeweils getrennt spezifizieren: weitere Fillet-/Chamfer-/Holevarianten, Taper an der Extrusion und Featuremuster. Drehen, Wandstärke, Formschräge, Folgen, Ausformung, Wendel und weitere Grundkörper sind seit 14.10.2026 eigene Pakete (M19–M27).
  2. Benannte Parameter/Ausdrücke mit Einheiten, Zyklenerkennung, Abhängigkeitsverfolgung und Fehlerdiagnose als eigenes Daten-/Evaluatorpaket entwerfen.
  3. Pro Feature die bereits verwendete Kette Spec→Kernel→History→Store→Controller→Edit→Abnahme mit analytischen Fixtures und Dateieinstiegen ausschreiben.
- **Abnahme:** Jeder tatsächlich eingeplante Befehl erhält eigenständige kleine IDs, positive/negative Fixtures und Freigabegrenze; sichtbare Schaltfläche bleibt bis vollständiger Umsetzung deaktiviert.
- **Grenze:** Dieses Paket liefert ausführbare Folgeplanung, nicht eine pauschale 'alles Solid fertig'-Behauptung.

## X20 – Jedes weitere Modul als Produkt separat abnehmen

- **Voraussetzungen:** X03, X04, X06, X08, X10, X14, X18, X19.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `docs/development-status.md`, `docs/dependencies.md`, `docs/provenance/README.md`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Für jedes ausgelieferte Modul seine Fixtures, Referenzprojekte, Migrations-/Plattform-/Paketchecks und Benutzeranleitung ausführen.
  2. Gemeinsame Releasepflichten L01–L10 wiederanwenden; Modulgrenzen und Interoperabilität prüfen.
  3. Weiteren Umfang erst nach tatsächlicher Nutzung/Fehlerdaten priorisieren und zu neuen kleinen Paketen konkretisieren.
- **Abnahme:** Kein Modul gilt durch registrierten Tab als fertig. Assembly/Drawings/Surface/Mesh/SheetMetal/Vault/Manufacturing haben getrennte nachweisbare Abnahme und bekannte Grenzen. Interoperabilitätsprojekte bleiben über Versionen lesbar.
- **Grenze:** Module können einzeln nach ihren Voraussetzungen veröffentlicht werden; der Gesamtnachweis X20 wartet auf alle hier geplanten ersten Lieferstufen.
