# 06 – Plattform und Veröffentlichung

P01–P04 sichern die frühe FreeBSD/Sway-Entwicklungsbasis. Gemäß Nutzerentscheidung und ADR 0007 kommt Meridian erst abschließend in L07 hinzu, um CAD- und Compositorfehler getrennt zu prüfen. L01–L10 enthalten die abschließende Produkt-/Paketabnahme.

**Prüfweg:** jeder Bericht enthält Revision, reale Umgebung und Resultat pro Fall. Der Agent führt Starttests, Interaktions-/Compositorprüfungen und eigene optische Kontrolle soweit technisch möglich selbst aus; nativ Wayland ausdrücklich nachweisen. Kapitel 10 regelt verfügbare Testtreiber, Aufnahmen und die minimale Nutzerbeteiligung. Nur nicht zugängliche oder nicht zuverlässig beobachtbare Hardwarewirkungen bleiben als externe Restabnahme offen.

## P01 – Plattformnachweis und Testmatrix vorbereiten

- **Voraussetzungen:** A07, A08.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `docs/freebsd-meridian.md`, `tools/run-freebsd-integration.sh`, `CMakePresets.json`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Matrix Windows Qt-OpenGL, Windows Native-Diagnose, FreeBSD Sway-Kontrolle, FreeBSD Meridian und Linux Core definieren.
  2. Für jeden Lauf Commit, Compiler, Qt/OCCT, QPA, GL/GPU, Display/DPI, Buildpreset und Paketstatus protokollieren.
  3. FreeBSD/Sway-Entwicklungsbasis von abschließender Meridian-Integration in L07 trennen (ADR 0007). Zugänge vor Blockermeldungen prüfen.
- **Abnahme:** Ein maschinenlesbarer Bericht besitzt alle Felder und kennzeichnet skipped/blocked getrennt von passed. Zwei Qt-OpenGL-Presets zählen nicht als zwei Backends. Keine behauptete heutige Meridian-Abnahme aus historischen Screenshots. Verfügbare Hostzugänge, UI-Testtreiber, Aufnahme- und Bildlesewege werden vor Nutzeranfragen inventarisiert; fehlende Fähigkeiten bleiben ausdrücklich offen.
- **Grenze:** Kein automatisch gestarteter unbekannter Remotehost; reale verfügbare Zielumgebung verwenden.

## P02 – Viewport-Gemeinsamkeiten und GL-Lebensdauer ordnen

- **Voraussetzungen:** A03, P01.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `src/viewport/CadViewport.cpp`, `src/viewport/CadViewportQtOpenGl.cpp`, `src/viewport/QtOcctGlContext.cpp`, `include/voelund/viewport/CadViewport.hpp`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Szene, Kamera-/Auswahllogik und Eingaberouting in gemeinsame Komponenten zerlegen; Backendgrenzen klein halten.
  2. Qt-Kontextneuerzeugung mit aboutToBeDestroyed, idempotentem Cleanup und Reinitialisierung behandeln.
  3. fitView/Projection/Redraw auf update()/zulässige GL-Callbacks oder explizite makeCurrent-Guard umstellen.
- **Abnahme:** Keine fachlich doppelte Szene-/Inputimplementierung. Wiederholtes Erstellen/Schließen und Reparent/Context-Recreate ohne stale GL-Ressourcen. Bestehendes Picking/Manipulatorverhalten erhalten.
- **Grenze:** Refactor in P02a gemeinsame Szene und P02b Lebenszyklus aufteilen, wenn Scopegrenze überschritten; keine neue Renderengine.

## P03 – DPI, FBO und Renderstabilität prüfen

- **Voraussetzungen:** P02.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `src/viewport/CadViewportQtOpenGl.cpp`, `src/viewport/QtOcctGlContext.cpp`, `tests/CMakeLists.txt`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Framebufferwechsel, Resize und devicePixelRatio-Änderung samt OCCT-Fenstermaßen korrekt propagieren.
  2. Gezielte Tests/Diagnose für Minimieren, Maximieren, Bildschirmwechsel und Context-Recreate ergänzen.
  3. Pickingkoordinaten und Overlaypositionen an denselben physischen/logischen Pixelvertrag binden.
- **Abnahme:** A-GL-01 besteht bei100/150/200% Skalierung, soweit Hardware/Compositor verfügbar. Known-point-Picking bleibt richtig; kein schwarzer/staler Viewport nach Wiederherstellung. Reale zugängliche Fälle selbst ausführen und optisch prüfen; nur technisch nicht selbst beobachtbare Restfälle für eine gezielte Nutzerprüfung offenhalten.
- **Grenze:** Keine feste Skalierung erzwingen, um Fehler zu verstecken.

## P04 – Native FreeBSD/Sway-Entwicklungsbasis abnehmen

- **Voraussetzungen:** P03.
- **Pflichtlektüre/Einstieg:** ADR 0007, Hostdokumentation, tests/desktop und tools/run-freebsd-integration.sh.
- **Auftrag:**
  1. Aktuellen Stand separat auf echtem FreeBSD/Sway mit nativem Wayland bauen; Build/Runtime/QPA/GL-Evidenz erfassen.
  2. Vollständigen CTest und produktive GUI-Smokes ausführen; Lifecycle, Picking, Orbit/Pan/Zoom, Manipulator sowie Größenwechsel und Overlays prüfen.
  3. Qt- und Compositoraufnahmen betrachten; Ergebnisse und Grenzen dokumentieren. Meridian gemäß ADR 0007 erst in L07 integrieren.
- **Abnahme:** Aktueller FreeBSD/Sway-Build, vollständiger CTest, GUI-Smokes und Lifecycle-/Interaktionsprüfung bestanden; native Wayland-/GPU-Umgebung und betrachtete Aufnahmen belegt. Kein XWayland-Ersatz. Hardwaregrenzen und GL-Diagnostik bleiben sichtbar dokumentiert.
- **Grenze:** Entwicklungsbasis gemäß ADR 0007, keine Meridian-Abnahme. Portal/Theme/Resume, physischer Mixed-DPI-Wechsel und abschließende Meridian-Releaseintegration bleiben L07.

## L01 – Verpflichtende CI und saubere Release-Buildmatrix

- **Voraussetzungen:** A04, A05, A07, M18.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `CMakePresets.json`, `CMakeLists.txt`, `tests/CMakeLists.txt`, `docs/dependencies.md`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. CI-Konfiguration im tatsächlich verwendeten Repositorysystem anlegen; falls keines existiert, zuerst runnerunabhängige Prüfskripte plus bestätigte Runnerzuordnung festhalten.
  2. Debug/Release auf Windows, FreeBSD und Linux Core konfigurieren; Format, Analyse, Grenzen, Tests und Plattformberichte als Gates behandeln.
  3. Nicht verfügbare GPU-/Meridianprüfungen als gesonderte erforderliche Abnahme behandeln, nicht stumm skippen.
- **Abnahme:** Clean checkout ohne bestehendes build/ baut reproduzierbar auf vorhandenen Runnern. Fehler in einem Pflichtcheck blockiert Freigabe; JUnit/Logs/Umgebungsmanifest werden je Commit archiviert. UI-Bilder und visuelle Vergleichsbefunde werden soweit betroffen mit Szenario/Umgebung archiviert; automatisch erzeugte Bilder nicht als bereits betrachtet kennzeichnen.
- **Grenze:** Kein grünes Badge mit fehlenden Pflichtplattformen; Netzwerk-/Runnerzugang ist explizite externe Voraussetzung.

## L02 – Laufzeitpaket und native Dateizuordnung bauen

- **Voraussetzungen:** L01, U12, M17.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `apps/voelund-cad/CMakeLists.txt`, `src/desktop/DocumentActions.cpp`, `CMakeLists.txt`, `docs/dependencies.md`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Installregeln für tatsächlich benötigte Qt-Plugins, OCCT/Compiler-Runtime, Ressourcen und Desktop-Dateien erstellen.
  2. Dateiargumente im produktiven main verarbeiten; mehrere Argumente eindeutig erlauben oder mit verständlicher Meldung begrenzen.
  3. Portable/Staging-Paket auf Rechner ohne Dev-PATH starten; Unicodepfade und native Dialoge prüfen.
- **Abnahme:** Installiertes voelund-cad öffnet per Dateizuordnung eine .volund-Datei mit Umlaut/Leerzeichen. Keine Abhängigkeit auf .tools oder Entwicklerarbeitsverzeichnis. Fehlende Runtime wird im Pakettest erkannt.
- **Grenze:** Installationsform pro Plattform vorab konkret festlegen; keine ungeprüften DLL-Sammelkopien.

## L03 – Release-Metadaten und Upgradepfad

- **Voraussetzungen:** L02, D14.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `apps/voelund-cad/main.cpp`, `CMakeLists.txt`, `docs/development-status.md`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Produktversion, Buildrevision, Schema-/Dependency-Versionen und About-/Diagnoseansicht ergänzen.
  2. Install/Upgrade/Uninstall-Regeln definieren, Benutzerdateien und Recoverydaten erhalten.
  3. Neue und alte Dokumente durch installierte Release-App öffnen und erneut speichern; Zukunftsformat verständlich ablehnen.
- **Abnahme:** Version aus Paket und Laufzeit stimmt überein. Upgrade erhält Einstellungen/Dokumente; Uninstall löscht keine CAD-Dateien. Migration nicht nur im Testbinary bestanden.
- **Grenze:** Keine automatische Rückwärtskompatibilität behaupten, die D08/D14 nicht nachgewiesen haben.

## L04 – Herkunft, Dependencybudget und Auslieferungsbestandteile abschließen

- **Voraussetzungen:** L02.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `docs/provenance/README.md`, `docs/provenance/occt-samples-qt.md`, `docs/provenance/freecad-planegcs.md`, `docs/dependencies.md`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Alle ausgelieferten Dependencies, Versionen, Quellherkünfte, Änderungen und Lizenztexte inventarisieren; eigene Adapter separat markieren.
  2. Vorhandene ADR-Anforderungen zur PlaneGCS-Verlinkung und notwendigen Weitergabematerialien in konkrete Paketdateien umsetzen; ungeklärte Release-Verlinkungsentscheidung ausdrücklich abschließen.
  3. Manifest/SBOM aus realem Staginginhalt erzeugen und mit Paket vergleichen; vollständige erforderliche Notices statt bloßer URLs beilegen.
- **Abnahme:** Jede ausgelieferte Bibliothek/Ressource hat zuordenbare Herkunft und vorgesehene Begleitmaterialien; fehlende Datei macht Paketprüfung rot. Keine Behauptung rechtlicher Freigabe allein aufgrund eines Toolchecks.
- **Grenze:** Neue Lizenz-/Verlinkungsfragen als fachliche Releaseentscheidung bearbeiten; keine eigene Rechtsberatung oder automatische Zustimmung erfinden.

## L05 – Autosave und Wiederherstellung ohne Originalverlust

- **Voraussetzungen:** D03, D13, U07.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `src/application/CadSession.cpp`, `src/desktop/DocumentActions.cpp`, `src/platform/README.md`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Recovery-Snapshots getrennt vom Benutzerpfad mit DocumentId und Ausgangsrevision speichern; Editor-Entwürfe von bestätigtem Dokument unterscheiden.
  2. Beim Start neue/ältere Recoverykandidaten erkennen, Unterschiede anzeigen und bewusst wiederherstellen/verwerfen lassen.
  3. Stale-Tempdateien, mehrere Appinstanzen und Schreibfehler behandeln; Originalspeicherung bleibt AtomicFileStore.
- **Abnahme:** A-RECOVERY-01: Abbruch nach Änderungen führt zu verfügbarem Recovery, Original bleibt unverändert; Restore kann als neue Datei gespeichert werden. Beschädigter Snapshot blockiert normales Öffnen nicht.
- **Grenze:** Kein stilles Überschreiben der letzten manuell gespeicherten Datei.

## L06 – Performance, Ressourcen und lange Modellhistorien absichern

- **Voraussetzungen:** M18, L01.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `src/features/FeatureGraph.cpp`, `src/features/SketchConstraintSolver.cpp`, `src/viewport/SketchPresentation.cpp`, `tests/CMakeLists.txt`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Deterministische Modellsets mit gestuften Element-/Feature-/Bodyzahlen und Referenzhardware definieren; Laufzeit, Speicher und Interaktionslatenz messen.
  2. Nur gemessene Engpässe optimieren: inkrementelle Auswertung, Caches nach Revision, Tessellierung; Ergebnisse dürfen Semantik nicht ändern.
  3. Bei erforderlicher Hintergrundauswertung Snapshot-/Revisionvertrag und kooperative Abbruchpunkte einführen; keine veralteten Ergebnisse publizieren.
- **Abnahme:** A-PERF-01 liefert reproduzierbare Messwerte und vereinbarte Grenzwerte vor Release. Abbruch/Neueingabe lässt UI korrigierbar; Memorywachstum nach wiederholtem Open/Close wird gemessen. Kein willkürliches Hardwareversprechen.
- **Grenze:** Keine premature Threadpool-Architektur; einzelne nicht abbrechbare OCCT-Aufrufe als konkrete Grenze dokumentieren.

## L07 – Vollständige Meridian-Produktabnahme wiederholen

- **Voraussetzungen:** P04, L02, L05, L06.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `docs/freebsd-meridian.md`, `tools/run-freebsd-integration.sh`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Installiertes Releasepaket auf echter Zielumgebung mit aktuellem OCCT/OCAF/DataExchange-Profil prüfen.
  2. Portal-Dateidialog, Clipboard, Theme, Tastaturfokus, Skalierung, Multimonitor, Maximieren, Suspend/Resume und Kontextwiederaufbau ausführen.
  3. A-PART-01 samt Save/Open/STEP auf Meridian durchführen; Sway nur zur Ursachenabgrenzung heranziehen.
- **Abnahme:** Alle in A-PLAT-01 vorgeschriebenen Checks besitzen pass/fail und Evidenz. QPA ist nativ Wayland; kein übersehener X-Fallback. Fehler werden mit reproduzierbarem Unterpaket behoben und nachgeprüft. Der Agent bedient und prüft die verfügbare Zielumgebung selbst einschließlich Bildkontrolle; nur unbeobachtbare Restwirkungen werden gemäß Kapitel 10 gebündelt an den Nutzer gegeben.
- **Grenze:** Initiales P04 allein reicht nach großen Backend-/Dependencyänderungen nicht als Releasefreigabe.

## L08 – Windows-Release und Linux-Core-Kompatibilität abnehmen

- **Voraussetzungen:** L02, L05, L06.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `tools/setup-windows.ps1`, `tools/setup-linux.sh`, `CMakePresets.json`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Windows-Stagingpaket auf sauberer Benutzerumgebung ohne Dev-PATH öffnen, modellieren, speichern und exportieren.
  2. Qt-OpenGL Produktweg vollständig prüfen; Native-Diagnose separat kennzeichnen.
  3. Linux-Core Build/CTest mit tatsächlich dokumentierter Dependencystrategie durchführen und Ergebnisse archivieren.
- **Abnahme:** Windows A-PART-01/02 und Unicode-Dateifälle bestanden; Linux-Core-Tests grün. Native-Windows nicht als Produktvoraussetzung ausgeben, wenn ADR nur Diagnose verlangt. Fehlende Plattform bleibt offen. Eigenständige optische Prüfung des Windows-Produkts nach A-VIS-01; keine generelle Bitte an den Nutzer, die Anwendung durchzutesten.
- **Grenze:** Keine Windows-Grafikfreigabe aus Headless-Coretests ableiten.

## L09 – Benutzerdokumentation und ausführbare Tutorials

- **Voraussetzungen:** L03, L04, L07, L08.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `README.md`, `docs/development-status.md`, `docs/sketch-data-model.md`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Installation, erstes Teil, Skizzenzustände, Timeline, Reparatur, Save/Recovery und STEP verständlich dokumentieren.
  2. Tutorials exakt aus getesteten Referenzmodellen ableiten; Screenshots aus installiertem Produkt mit tatsächlichen Features.
  3. Featurematrix und bekannte Einschränkungen aktualisieren; historische Prototypanleitungen klar trennen.
- **Abnahme:** Der Agent arbeitet A-PART-01 ausschließlich anhand des Tutorials gegen das installierte Produkt ab, dokumentiert Lücken und korrigiert sie. Eine zusätzliche Erstnutzerprobe ist optional; die Eigenprüfung wird nicht als unabhängiger Usability-Test ausgegeben. Kein Screenshot/Schritt benutzt deaktivierten Befehl. Architektur- und Userdoku widersprechen sich nicht.
- **Grenze:** Keine pauschale Behauptung 'FreeCAD-Ersatz' ohne Funktionsabdeckung.

## L10 – V1-Releasekandidat prüfen und nachvollziehbar freigeben

- **Voraussetzungen:** L09.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `docs/development-status.md`, `docs/implementation-plan/08-abnahme.md`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Releasekandidat mit unveränderlicher Revision, Dependencymanifest, Prüfergebnissen und installierbaren Artefakten zusammenstellen.
  2. Offene Befunde nach Datenverlust/Modellrichtigkeit/Bedienbarkeit prüfen; P0/P1 im v1-Umfang müssen erledigt oder Produktumfang ausdrücklich neu entschieden sein.
  3. Vollständiges Gateprotokoll erstellen; tatsächliche Veröffentlichung/Signierung im vorgesehenen autorisierten Prozess ausführen.
- **Abnahme:** G0–G4 belegt, alle Pflichtszenarien bestanden, keine als grün ausgegebenen skipped Gates. Paket installierbar und Dokumente migrierbar. Veröffentlichung erst nach projektspezifischer Releasefreigabe, falls dafür gesonderte Autorisierung nötig ist.
- **Grenze:** Ein fertig gebauter Releasekandidat allein wird nicht als bereits veröffentlicht gemeldet.
