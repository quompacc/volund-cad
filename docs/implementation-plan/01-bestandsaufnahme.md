# 01 – Bestandsaufnahme

Untersucht am 13.09.2026 gegen `1b78229`. Die folgenden Befunde beschreiben diesen Stand, nicht bereits ausgeführte Reparaturen.

## Umfang und Autorität

Gelesen wurden die 17 Markdown-Dokumente unter `docs`, Root- und Subsystem-READMEs, die ursprüngliche Projekt-/Architekturvereinbarung als DOCX-Text, Build-/Setup-Konfigurationen und der eigene Anwendungscode einschließlich Tests. Das Inventar vor diesen Planungsdateien umfasst 220 versionierte Dateien; 148 relevante eigene Code-/Builddateien unter `apps`, `include`, `src`, `tests`, `tools`, `cmake` umfassen 20.008 physische Zeilen. Das ist eine Architektur- und Verhaltensprüfung, kein formaler Korrektheitsbeweis jeder Zeile. Unveränderten PlaneGCS-Upstream und den eingefrorenen FreeCAD-Spike habe ich als Abhängigkeit bzw. historische Referenz eingeordnet, nicht vollständig algorithmisch auditiert.

Autoritätsreihenfolge:

1. Aktueller [Architekturvertrag](../architecture-and-code-contract.md), Version 1.2, und spätere einschlägige ADRs.
2. ADR 0004: FreeBSD/Meridian als Produktziel; ADR 0006: Qt besitzt den OpenGL-Kontext.
3. Skizzen-Datenmodell und Statusdokument für konkret beschriebene Daten/Implementierung; Behauptungen werden am Code geprüft.
4. Ursprüngliche DOCX-Vision: CAD/Vault/Manufacturing, parametrische Timeline, gemeinsame Identität. Überholte Vorschläge zu FreeCAD-Laufzeit, QML/Rust oder Python werden nicht wieder eingeführt.
5. Phase-0-Dokumente und Spike sind historische Evidenz, keine aktuelle Bauanleitung.

## Was weiterverwendet werden soll

- C++20, Qt Widgets, OCCT und PlaneGCS sowie die vorhandene Schichtung.
- Geometrieprimitiven, Constraint-Adapter und die vorhandenen positiven Integrationsfälle.
- `CadSession`, `SketchEditorSession`, Registry und generischer ToolPanelHost als Ausgangspunkte; Fehler und Verantwortlichkeiten gezielt bereinigen.
- Die genehmigte Gestaltung der neueren Vorschau inklusive Navigation. Visuelle Zusammenführung statt Neugestaltung.
- `.volund`-Lesekompatibilität für Versionen 1–16 und die bestehenden Dokumentidentitäten, soweit bereits vorhanden.
- Die historische Sway/FreeBSD-Evidenz als Kontrollmessung. Sie ersetzt keine Meridian-Abnahme.

## Befunde mit Codebezug

| ID / Priorität | Befund und Auswirkung | Beleg / Status | Pakete |
|---|---|---|---|
| F01 / P0 | Source-Limit-Test kann null Dateien prüfen und trotzdem bestehen. | `tests/CMakeLists.txt`, Root-CMake und `tools/cmake/CheckSourceLimits.cmake`: literal übergebene Anführungszeichen; direkt reproduziert. | A01–A03 |
| F02 / P0 | Speichern kürzt die vorhandene Datei vor erfolgreichem Schreiben; Dirty wird vor expliziter Abschlussprüfung gelöscht. | `CadSession::saveDocument`, `std::ofstream(...trunc)`; Quellcodebeleg. | D01–D03 |
| F03 / P0 | Eine zusammengesetzte Benutzeraktion erzeugt mehrere Dokument-Undo-Schritte. | `addSketchExtrusion`; Integrationstest erwartet ausdrücklich zwei Undos. | D04–D06 |
| F04 / P0 | Modelländerung kann trotz fehlgeschlagenem Recompute als Erfolg zurückkommen. | `CadSession.cpp`, verworfene `recompute()`-Rückgaben; FeatureGraph behält letzte ganze gültige Szene. | D04, D07, R01 |
| F05 / P0 | Fehlgeschlagenes Skizzenlösen wird nicht überall vom Akzeptieren unterschieden; Setzen editierter Parameter wird teils ungeprüft beendet. | `SketchEditorSession::canAccept`, `confirmPreview`, `accept`. Statisch festgestellt; gezielte Regression fehlt. | D07, S02 |
| F06 / P0 | Flächen-/Kantenreferenzen können unbemerkt auf andere Geometrie wechseln. | `SketchSupportResolver`: Normalen/Nähe; Support-Kanten als Segmentindex in `auxiliaryValues`; `SketchInteraction` dupliziert Auflösung. | R02–R06 |
| F07 / P1 | OCAF ist Vertragsanforderung, aber keine implementierte Dokumentbasis. | ModelDocument nutzt Vektoren und State-Kopien; keine OCAF-Dokument-/Label-API im eigenen Code; FreeBSD-OCCT-Build deaktiviert ApplicationFramework. | D08–D14 |
| F08 / P1 | Profilerkennung ist keine allgemeine Regionserkennung. Verschachtelte Kreise ergeben getrennte Flächen statt Ringregion. | `SketchProfiles`, `SketchEvaluation`: greedy Konturverkettung und Einzel-Faces; Codebeleg, analytischer Regressionstest zu ergänzen. | R07–R10 |
| F09 / P1 | Preview-Extrude-OK schließt nur die Ansicht; Parameter steuern keine entsprechende Extrusion. | `PreviewWindow.cpp`, `activateExtrude`, `closeActiveTool`. | U01–U08, M02–M04 |
| F10 / P1 | Timeline/Titel sind fest verdrahtet; mehrere Workspaces und Buttons ohne Fachfunktion. | `PreviewWindow`, `PreviewWindowRibbon`; statische Einträge und fehlende Handler. | U01, U06, M15 |
| F11 / P1 | Zwei Anwendungsschalen und zwei Skizzen-Erstellungswege weichen voneinander ab. | Legacy `SketchEditorCreation` gegenüber Preview `SketchBackend`; Registrierung und ToolPanelHost im Preview umgangen. | U02–U12, S01 |
| F12 / P1 | Mehrere neue Skizzenwerkzeuge sind nicht in allen Auswahl-/Snap-/Labelpfaden vollständig integriert. | Ellipse mit unpassender Startpunktrolle, Rechteckzweitecke, Arc-Endpunktreihenfolge, Slot-Rollen; Vergleich der zwei Erstellungswege. | S01, S03–S07 |
| F13 / P1 | Mirror hält einen Zeiger in einen Vektor, in den danach eingefügt wird. Reallokation kann den Zeiger ungültig machen. | `SketchEditorOperations::mirrorSelected`; statisches Lebensdauerrisiko, hier kein Crash reproduziert. | S08 |
| F14 / P1 | Ellipsenoffset durch Vergrößern beider Halbachsen ist kein geometrisch konstanter Offset. | `SketchEditorOperations`; mathematisch falsche Semantik für einen allgemeinen Offset. | S09 |
| F15 / P1 | Undo-Grenzen entstehen teilweise beim Refresh oder pro Drag-Move. | `SketchEditorPresentation::refreshPresentation`, Preview-Translate-Pfad. | S02, U09 |
| F16 / P1 | Feature-Liste und ein einzelnes Input-Feld sind noch kein allgemeiner Abhängigkeitsgraph; Feature-ID fungiert teils als Körperidentität. | `FeatureGraph`, `findBody`, `inputFeature`; historische Inputs können nachfolgende Körperauflösung stören. | D11, R01, M01 |
| F17 / P1 | OCCT-Fehler werden nicht durchgehend an der Kernelgrenze vereinheitlicht. | ShapeFactory/Operations ohne `Standard_Failure`-Abfang; FeatureGraph fängt `std::exception`, OCCT-7.9-Kompatibilität benötigt eigenen Pfad. | A06, D07 |
| F18 / P1 | Redraw ist nicht überall an aktuellen Qt-Kontext gebunden; Kontextneuerzeugung hat keinen vollständigen Cleanup-Pfad. | `CadViewportQtOpenGl::setPerspectiveProjection`, `fitView`, Destruktor; kein `aboutToBeDestroyed`-Handler. | P02, P03 |
| F19 / P1 | Zwei große Viewportimplementierungen duplizieren Szene und Eingabe. | `CadViewport.cpp`, `CadViewportQtOpenGl.cpp`; 698/699 Zeilen. | A03, P02 |
| F20 / P1 | Leeres Dokument → Basisebenenskizze → erster eigener Extrusionskörper fehlt als durchgehender Produktablauf. | Skizze benötigt planare Körperfläche, Extrusion verbindet mit vorhandenem Körper; positive Distanz/Fuse-Fokus. | S03, M01–M04 |
| F21 / P1 | Produkt-Preview wird nicht als eigentliche App installiert; Dateizuordnung behauptet `%F`, Einstieg verarbeitet nur Smoke-Flag. | `apps/*/CMakeLists.txt`, `apps/voelund-cad/main.cpp`, Desktop-Datei. | U12, L02, L03 |
| F22 / P1 | Format-/Lizenz-/Schicht-/Komplexitätsregeln sind nicht vollständig automatisiert. | `.clang-tidy` ohne verpflichtenden Lauf; keine CI; regression/differential nur READMEs. | A02, A04, A05, L01, L04 |
| F23 / P1 | Alte Formatversionen und beschädigte Dateien sind nur ausschnittsweise getestet. | Serializer deckt 1–16 ab, explizite Altversionstests insbesondere 1/2/4, keine vollständige Fixturematrix. | D02, D09, D14 |
| F24 / P1 | Windows-Presets mit verschiedenen Namen testen aktuell denselben Qt-OpenGL-Backend. | `CMakePresets.json`; kein Nachweis für zwei Backends aus zwei grünen Verzeichnissen ableiten. | A07, P01 |
| F25 / P1 | Finales Meridian-Gate und installierte Release-Laufzeit nicht nachgewiesen. | `docs/freebsd-meridian.md`, Integrationsskript: Build/Smoke/ldd deckt manuelle Checkliste nicht ab. | P04, L01–L10 |
| F26 / P2 | Ausgewertete und gespeicherte Skizzenzustände können auseinanderliegen. | FeatureGraph löst Kopien, selectedSketch*-APIs lesen ursprüngliche Parameter. | D12, S07 |
| F27 / P2 | Einige mutierende/noexcept-Pfade können werfen; Validierung ist uneinheitlich. | ModelDocument State-Kopien in noexcept Undo/Restore; endliche Werte/ID-Überlauf/Ebenennormierung nicht überall geprüft. | A06, D05, D10 |
| F28 / P2 | Legacy-Rechtecke können beliebige Rotation nicht darstellen; Ellipsenauswahl wird nicht durch alle Transformationen getragen. | Zwei-Ecken-Rechteckdaten; Translate-Auswahlzweige und Validierungs-/Rollbackpfade. | S08, S10 |
| F29 / P2 | Preview-Smoke testet Backend vor Fensteraufbau und Sichtbarkeit, aber keine echte Benutzer-Befehlskette. | `apps/voelund-ui-preview/main.cpp`, `smokeTestPassed`; keine Rendering-/QPA-Abnahme. | A08, U11 |
| F30 / P2 | Deployment- und Dependency-Unterlagen laufen Implementierung teils hinterher. | Linux-Setup/vcpkg versus Preset; FreeBSD-Prereqs; lean OCCT ohne benötigte OCAF-/Austauschmodule; Herkunftsauslieferung unvollständig. | A07, D08, L02, L04 |

Prioritäten: P0 = Datenverlust/falscher Erfolg zuerst verhindern; P1 = blockiert belastbares Produkt; P2 = Pflichtkorrektur in seinem Arbeitsbereich. P0 bedeutet hier nicht, dass ein produktiver Schadensfall beobachtet wurde.

## Durchgeführte Prüfungen

| Prüfung | Ergebnis und Grenze |
|---|---|
| Arbeitsbaum vor Planung | Sauber; HEAD `1b78229` |
| Vorhandenes Windows-CTest-Preset | Zunächst nicht lauffähig, weil Cache auf `V:/` zeigte und Mapping in der Ausführungsumgebung fehlte. Umgebungseffekt, kein CAD-Testfehler. |
| Frische Konfiguration mit echtem Unicode-Quellpfad | Konfiguration erfolgreich; Link scheiterte am Boost-Library-Pfad mit verstümmeltem `Völund` (`LNK1181`). Reproduzierbarkeitsproblem bleibt zu dokumentieren/beheben. |
| Frischer Build über geprüftes `V:`-Mapping | `build/audit-ascii-20260913`, Qt-OpenGL, MSVC 19.51, Debug, vorhandene Dependencies: 210/210 Buildschritte erfolgreich. Kein vollständiger Dependency-Neuaufbau. |
| Frisches CTest | 16/16 bestanden, ca. 9 Sekunden; darunter zwei Desktop-Start-Smokes. |
| Direkter Source-Limit-Check | Korrektes `-DVOELUND_SOURCE_ROOT=D:/300_Projekte/310_Aktiv/Völund` ergibt Fehler. Mit eingebetteten Anführungszeichen ergibt sich fälschlich Erfolg. |
| FreeBSD/Meridian, Linux, Release-Paket | In dieser Untersuchung nicht ausgeführt. Historische Dokumentation nicht als heutiger Nachweis umgedeutet. |

Direkt beanstandete Dateien:

| Datei | Physische Zeilen | Vertragslimit |
|---|---:|---:|
| `src/document/DocumentSerializer.cpp` | 601 | 600 |
| `src/viewport/CadViewport.cpp` | 698 | 600 |
| `src/viewport/CadViewportQtOpenGl.cpp` | 699 | 600 |
| `tests/integration/CadSessionTests.cpp` | 658 | 600 |

Vorhandene Tests: `desktop.viewport_startup`, `desktop.ui_preview_startup`, `kernel.shape_factory`, `document.history`, `features.sketch_profiles`, `features.planegcs_solver`, `features.sketch_constraint_adapter`, `application.recompute_history`, `application.sketch_editor_flow`, `application.sketch_construction`, `application.sketch_circle_constraints`, `application.sketch_alignments`, `application.sketch_fix_distance`, `application.sketch_basic_tools`, `workspaces.module_registry`, `architecture.source_limits`.

Lokale Rohbelege liegen in den ignorierten Build-Artefakten `build/audit-configure.log`, `build/audit-build.log`, `build/audit-tests.xml`. Sie sind keine dauerhafte CI-Ablage. A07/A08 und L01 machen diese Evidenz reproduzierbar und archivieren sie künftig pro Revision.

## Anforderungsabdeckung

| Ursprung | Anforderung | Planstelle |
|---|---|---|
| Vertrag / ADR 0002, 0003 | Schichten, registrierte Module, temporäre Panels, OK/Cancel | Zielverträge K01–K04; U01–U12 |
| Vertrag | OCAF, Persistenz, Undo/Redo, stabile Identitäten | K03–K08; D01–D14, R01–R06 |
| Vertrag | Größen/Komplexitätsgrenzen, Tests, Provenienz, kein Python | A01–A08, L01–L04 |
| ADR 0004 / 0006 | Meridian nativ Wayland, Qt-OpenGL | P01–P04, L06–L08 |
| Skizzenmodell / Status | Werkzeuge, Constraints, Regionen, Migration 1–16 | R07–R10, S01–S14, D09/D14 |
| Ursprüngliche Vision | Feature-Timeline mit Edit/Suppress/Rollback/Fehlern | M13–M16 |
| Ursprüngliche Vision | Pocket, Hole, Fillet, Chamfer, Mirror/Pattern, Messung | M03–M12 |
| Ursprüngliche Vision | Gemeinsame Projekt-/Modell-/Revisions-/Artefaktidentität | D11, X11–X14 |
| Ursprüngliche Vision | Baugruppen, Zeichnungen, Fertigung, CAM | X01–X04, X15–X18 |
| Sichtbare spätere Workspaces | Surface, Mesh, Sheet Metal | X05–X10 |

Die wesentliche Lücke ist damit nachvollziehbar: Manche Anforderungen stehen bereits in Dokumenten oder Oberflächen, besitzen aber noch keinen vollständigen Daten-, Ausführungs- und Abnahmepfad.
