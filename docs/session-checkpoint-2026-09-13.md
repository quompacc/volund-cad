# Pause / Fortsetzung – 13. September 2026

Auf Nutzerwunsch pausiert. Keine Hintergrundarbeit und kein automatischer Start.
Nächster Arbeitsschritt bei Wiederaufnahme: **D07**. Nicht erneut D01–D06 beginnen.

## Verbindlicher Stand

- CAD nativ C++20, Qt Widgets, OCCT. Kein Python für Produkt, Build oder Tests.
- Entwicklung auf **FreeBSD/Sway**; Meridian ausdrücklich erst zum Schluss (L07),
  gemäß ADR 0007. Fehlender Meridian-Nachweis blockiert die CAD-Entwicklung nicht.
- Abgenommen: A01, A03, A05–A08, P01–P04, D01–D06. Details und Grenzen stehen im
  [Manifest](implementation-plan/packets.json) und den paketbezogenen Evidenzen.
- A02 bleibt needs-design (echter Clang-CFG-Consumer fehlt), A04 daher offen.
  G0 nicht pauschal als vollständig bestanden ausgeben.
- Letzte Produktabnahme D06: Windows **36/36** CTests, FreeBSD/Sway **32/32**.
  Aktuelle CAD-Aufnahmen auf beiden Plattformen betrachtet; neun D06-Quellhashes
  zwischen lokalem und entferntem Stand identisch.
- D05-Allokationsprüfung zusätzlich Windows Release **2/2**. MSVC Debug benötigt
  wegen allokierender noexcept-Iteratorproxies den dokumentierten Release-Nachweis.
- Keine neue fachliche Codeänderung nach den D06-Läufen; Pausenabschluss aktualisiert Dokumentation und bereinigt zusätzliche Leerzeilen am Dateiende.

## Einstieg D07

Zuerst [D06-Evidenz](implementation-plan/evidence/D06.md),
[Befehlsvertrag](command-transaction-contract.md),
[D07-Auftrag](implementation-plan/03-fundament.md) und K03/K05 lesen.

D06 hat die Dokumenttransaktion produktiv angeschlossen: AddSketchExtrusion erzeugt
mit Sketch und Extrude einen Undo-Schritt; separate Bestätigungen bleiben zwei.
Save→Edit→Undo ist clean. Vorschau/Auswahl und Viewportnavigation schreiben keine
History. CadSessionTransactions.cpp bereitet Auswertung vor dem Dokument-Commit vor.

D07 ist **noch nicht umgesetzt**:
1. Öffentliche bool-/FeatureId-Mutationsresultate und verworfene Recompute-/Setter-
   Ergebnisse in eindeutige Resultate überführen. D04-Typen sind bisher Vertrag,
   noch keine durchgängige produktive API.
2. FeatureGraph fängt derzeit breit std::exception und übersetzt in
   FeatureEvaluationError. Infrastruktur-/Programmierfehler nicht als geometrische
   Ablehnung verstecken; Ausnahmegrenzen und Qt-Callbacks prüfen.
3. Neue ungültige Kandidaten abweisen. Reparierbare Änderungen bestehender Features
   nur ausdrücklich als CommittedWithErrors übernehmen und betroffene IDs anzeigen.
4. Solver Failed von Conflicting/UnderConstrained trennen. Numerisch fehlgeschlagene
   Skizzen dürfen nicht still accepted werden; Editor muss korrigierbar bleiben.
5. Letzte gültige Geometrie ausdrücklich als stale mit passendem Stand kennzeichnen;
   Undo/Recompute und Save/Open fehlerhafter Modelle weiter prüfen. D06 erhält noch
   die bisherige Fehlersemantik. Echte DAG-Fehlerausbreitung ist erst R01.
6. Passende Fehler-, Reparatur-, Cancel-/Undo-Tests ergänzen, beide Plattformen bauen
   und testen, sichtbare Änderungen aufnehmen und selbst betrachten. Erst dann D07
   im Manifest abnehmen. Keine routinemäßige Nutzerprüfung oder Terra-Übergabe.

## Arbeitsumgebung und Reproduktion

Windows-Repository: `D:/300_Projekte/310_Aktiv/Völund`, aktueller Branch beim Pause-
Commit `main`. Der breite, zuvor uncommittete Gesamtstand wird bewusst als zusammen-
gehöriger Checkpoint gesichert, ohne rückwirkend einzelne Paketcommits zu erfinden.
Quellen, Fixturedateien und versionierte Evidenz werden eingecheckt. Buildordner,
.tools und private Schlüssel bleiben außerhalb von Git. Kein Push beauftragt.

Produktbuild: `build/p02-qt-final` (Name historisch, Inhalt aktuell).
`tools/setup-windows.ps1 -SkipConfigure -PassThru` liefert Buildkontext; V:-Mapping,
DeveloperShell, CMake und CTest im selben Prozesskontext verwenden.
Vollständige Tests: `tools/test-windows.ps1 -BuildDirectory build/p02-qt-final`.
Release-Allokationstest: `build/d05-release`, Target
`voelund_document_transaction_tests`, CTest-Filter `^document.transaction`.

FreeBSD-Zugang siehe [development-hosts.md](development-hosts.md):
`ssh -i "$env:USERPROFILE/.ssh/meridian_freebsd" -o IdentitiesOnly=yes -o BatchMode=yes eduard@192.168.1.238`.
Der Nutzer hat Transfers und separate Builds für diese Sitzung ausdrücklich
freigegeben. Keine Schlüsseldateien kopieren. Aktiver isolierter Quellbaum:
`/home/eduard/src/voelund-p04-20260913`, Preset/Build
`freebsd-clang-lean-occt`. Original `/home/eduard/src/voelund` unangetastet lassen.

Wayland-Läufe: `XDG_RUNTIME_DIR=/var/run/xdg/eduard`, `WAYLAND_DISPLAY=wayland-1`,
`QT_QPA_PLATFORM=wayland`, DISPLAY entfernen. Sitzung und IPC-Socket bei Wiederaufnahme
neu prüfen. Eigene Vollbildfenster dürfen Lifecycle-Smokes nicht verdecken.
Windows OCCT 8.0.1; FreeBSD OCCT 7.9.3 / Qt 6.11.1 / Sway 1.12.

## Offen, ohne die jetzige Entwicklung zu blockieren

A02/A04, P04-GL-Diagnosemeldungen, echter Mixed-DPI-Monitorwechsel, Portal-/Meridian-
Integration und die übrigen geplanten Pakete. Kein Anspruch auf fertiggestelltes
Gesamt-CAD. Die jüngsten belastbaren Nachweise sind in
[implementation-plan/README.md](implementation-plan/README.md) verlinkt.
Rohbelege in evidence behalten ihre Originalbytes (einschließlich CRLF und
protokollierter Leerzeichen); .gitattributes nimmt nur diesen Belegbaum von
Whitespace-Diagnosen aus. Die Commitprüfung der übrigen Dateien erkennt CRLF
als Zeilenende (core.whitespace=trailing-space,space-before-tab,cr-at-eol).
