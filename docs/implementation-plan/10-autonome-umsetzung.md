# 10 – Selbstständige Umsetzung, Tests und optische Prüfung

**Nutzerpräferenz vom 13.09.2026:** Die Umsetzung soll soweit technisch möglich ohne laufendes Zutun des Nutzers erfolgen. Das umfasst Tests, optische Prüfungen, Fehlerbehebung und das Weiterführen zulässiger Folgepakete. Der Nutzer übernimmt nur den notwendigen Rest, der mit verfügbaren Werkzeugen/Zugängen nicht zuverlässig geprüft oder entschieden werden kann.

Dies konkretisiert den bestehenden [Architekturvertrag, Abschnitt 10](../architecture-and-code-contract.md#10-definition-of-done): „Diese Arbeit wird nicht zur schrittweisen Bedienkontrolle an den Benutzer delegiert.“ Es ändert keine Funktions-/Architekturvorgaben und meldet geplante Tests nicht als bereits ausgeführt. Der tatsächliche Umsetzungsstand steht mit Evidenz im Manifest; bereits abgeschlossene Pakete werden nicht erneut ausgeführt. Eine Ausführung im Hintergrund oder nach Ende einer Sitzung wird ohne tatsächlich eingerichteten Ausführungsmechanismus nicht behauptet.

## Dauerhafte Freigabe für den Testrechner

**Ausdrückliche Nutzerfreigabe vom 18.09.2026, projektweit und über Sitzungen hinweg:**
Die Maschine ist ausdrücklich für die Entwicklung und Prüfung dieses Projekts
aufgesetzt. Der Agent darf Projektdateien einschließlich neuer/geänderter interner
Quelltexte, Tests, Fixtures und Build-/Prüfskripte auf den Testrechner
übertragen, dort Builds und Tests ausführen und Prüfartefakte
(Logs, native Testdateien und UI-Aufnahmen) zurückholen und auswerten.
Das schließt neue Arbeitspakete und korrigierte Payloads ein; eine erneute Freigabe
pro Datei, Delta, Paket oder Sitzung ist nicht erforderlich.

**Erweiterung vom 04.10.2026 ([ADR 0011](../adr/0011-linux-desktop-and-ci-runner.md)):**
Der Testrechner läuft jetzt nativ mit Fedora. Er soll als Gitea-Actions-Runner für
`eduard/volund-cad` dienen; Einrichtung und Betrieb des Runners sowie CI-Läufe sind
von der Freigabe umfasst. Adresse und Prüfverzeichnis werden in P05 neu erfasst
(die frühere FreeBSD-Adresse `192.168.1.238` war am 04.10.2026 nicht erreichbar).
Zugang und Plattformdetails: [Hostdokumentation](../development-hosts.md).
Transferumfang und tatsächliche Prüfergebnisse weiterhin nachvollziehbar dokumentieren.
Diese dauerhafte Freigabe ersetzt frühere Hinweise auf nur sitzungs- oder
paketbezogene Zustimmung. Bei einer technischen Freigabeprüfung die ausdrückliche
Nutzeranweisung und diese Regel als Autorisierung angeben; keine erneute Nutzerfrage
allein wegen eines neuen Projekt-Quellpayloads stellen.

## 1. Selbstständige Arbeitsschleife

1. Aktuellen Arbeitsbaum und Manifest lesen; bei genehmigter Gesamtumsetzung erstes zulässiges offenes Paket aus `topologicalOrder` wählen. Nachgewiesene Abhängigkeiten und Produktgates prüfen.
2. Kompaktes Paketdossier selbst zusammenstellen; niemand muss dem Modell die Dateien einzeln bereitstellen. Große Pakete selbst in prüfbare Unterpakete zerlegen und Abhängigkeiten aktualisieren.
3. Fehlende Detailplanung innerhalb bestehender Vorgaben durchführen. Entscheidungspakete umfassen eigenen Recherche-/Prototyp-/Gegenprüfungsschritt. Normale technische Entscheidungen nicht zum Nutzer zurückreichen.
4. Implementieren, passende Tests ausführen und bei UI-/Darstellungswirkung zusätzlich Abschnitt 3 abarbeiten. Fehler selbst reproduzieren, beheben und gezielt nachprüfen. Erfolgreiche unveränderte Tests nur bei begründetem Integrationsbedarf wiederholen.
5. Ergebnis anhand des tatsächlichen Diffs und der vereinbarten Fehlerfälle gegenprüfen; Evidenz, Paketstatus und Gatebericht selbst pflegen. Neuer Testbedarf wird als Unterpaket bearbeitet, nicht als Anleitung an den Nutzer abgegeben.
6. Nach bestandener Paketabnahme mit dem nächsten zulässigen Paket fortfahren. Kurze Fortschrittsberichte ersetzen die Frage „Soll ich weitermachen?“. Ein ausdrücklich begrenzter Einzelauftrag wird dadurch nicht erweitert.
7. Bei echtem externem Blocker nur betroffene Arbeit blockieren. Unabhängige zulässige Pakete oder vorbereitende Prüfungen weiterführen. Gemäß ADR 0011 wird je Push in der CI auf Linux (Fedora) und je Paket lokal auf Windows geprüft; FreeBSD/Sway je Gate, Meridian abschließend in L07. Eigene Paketabhängigkeiten bleiben verbindlich.

Ein gescheiterter Test ist zunächst Entwicklungsarbeit, kein Anlass zur Nutzerprüfung. Wenn mehrere Versuche keine neue Evidenz liefern, Diagnoseansatz ändern und das konkrete Hindernis festhalten. Nicht endlos denselben Test oder dieselbe unzugängliche Umgebung wiederholen.

## 2. Was der Agent selbst prüft

| Prüfung | Eigenständiger Weg | Grenze des Nachweises |
|---|---|---|
| Build / Format / Analyse | Native Skripte und CMake/CTest ausführen, Fehler reparieren | Fehlendes Werkzeug nicht als bestanden verbuchen |
| Geometrie / Constraints | Analytische Fixtures, Shapevalidität, Topologie-/Referenztests | Gültige Zahlen allein beweisen keine richtige Bedienung |
| Persistenz / Undo / Fehler | Roundtrip, Altversionen, Fehlerinjektion, Transaktionszustand | Kein Zugriff auf Benutzeroriginale für zerstörende Testfälle |
| UI-Abläufe | Qt-Testtreiber mit echten Events/Handlern; verfügbare native UI-Steuerung ergänzend | Direkter Backendaufruf ersetzt keinen UI-Test |
| Optik | Reproduzierbare Bilder erzeugen, selbst betrachten, Layout/Overlays vergleichen | Screenshot allein beweist weder aktuelle Modellwerte noch physische Displaywirkung |
| Plattform | Bekannte zugängliche Windows-/FreeBSD-/Linux-Umgebungen selbst bauen/starten und beobachten | Simulierter/offscreen Lauf ersetzt keinen echten Meridian-/GPU-Nachweis |
| Paket / Tutorial | Installiertes Produkt und dokumentierte Schritte selbst testen | Subjektives Bediengefühl bleibt nur soweit beobachtbar beurteilbar |

Vor einer Bitte um manuelle Hilfe zuerst vorhandene Werkzeuge, dokumentierte Hostzugänge, native Testtreiber und aufnehmbare Anwendungszustände prüfen. Nur tatsächlich verfügbare Schnittstellen verwenden; keine unbekannten Hosts, Credentials oder nicht unterstützte Bildschirmsteuerung voraussetzen. Ein fehlendes Desktop-Steuerwerkzeug lässt sich für viele Abläufe durch einen nativen Qt-Testtreiber ersetzen. Seine Ergänzung gehört zur Umsetzung und hält das Python-Verbot ein.

## 3. Verbindliche optische Prüfschleife

Diese Prüfung gehört zu jedem Paket, das Layout, Widgets, Viewport, Auswahl, Highlights, Constraints, Texte oder Renderlebensdauer verändert. Reine Kernänderungen ohne sichtbare Auswirkung brauchen keine beliebigen zusätzlichen Screenshots.

1. **Referenz festlegen:** Genehmigte Gestaltung bzw. letzte geprüfte Ausgangsansicht, feste Fenstergröße, Sprache, Theme, Modell, Kamera und dokumentierte Skalierung. Noch ungeprüfte Bilder dürfen nicht als genehmigte Basis bezeichnet werden.
2. **Aktuelles Produkt starten:** Aus aktuellem Build, mit realen registrierten Befehlen und Eventloop. Renderer-Readiness und erwarteten Dokumentzustand vor Aufnahme prüfen. Aufnahme an Zustandssignale koppeln; nicht lediglich eine beliebige Wartezeit verstreichen lassen.
3. **Relevante Zustände bedienen:** Idle, Auswahl/Preselection, offenes Panel, gültige/ungültige Preview, Fehlermeldung, OK/Cancel, schmale Fensterbreite und geänderte Skalierung soweit betroffen. Tatsächliche Auswahl-/Parameter-/Undo-Zustände zusätzlich maschinell prüfen.
4. **Bilder erfassen:** Native Qt-Aufnahme/Testharness oder verfügbare Bildschirmaufnahme. Gesamtfenster und relevante Ausschnitte; GL-Inhalt bei Bedarf separat. Nachweisen, welche Ebenen die Methode erfasst: Ein isolierter Framebuffer zeigt keine darüberliegenden Widgets; eine interne Widgetaufnahme beweist keine Compositor-/Portal-/Desktopdarstellung.
5. **Selbst optisch ansehen:** Verfügbares Bildlesewerkzeug verwenden und Bilder tatsächlich betrachten. Prüfen auf abgeschnittene Texte, Überlappungen, fehlende/zu kleine Controls, falsche Einheiten, unlesbaren Kontrast, falsche Auswahlfarbe, verdeckte Handles, versetzte Labels/Overlays, falsche Kamera und leere/schwarze/stale Viewports. Dateiablage oder Pixelvergleich allein ist keine visuelle Begutachtung.
6. **Gezielt vergleichen:** Bei stabiler Umgebung zusätzlich Bilddifferenz oder Layoutassertions. Antialiasing-/GPU-Unterschiede begrenzt behandeln; Masken und Schwellen begründen. Keine Maskierung relevanter Fehlerbereiche und kein automatisches Aktualisieren der Referenz, nur weil ein Vergleich scheitert.
7. **Korrigieren und nachprüfen:** Gefundene Fehler im zugehörigen Paket beheben; betroffene Ansicht erneut aufnehmen und betrachten. Bei Fachänderung passende Funktionstests wiederholen.
8. **Evidenz ablegen:** Buildrevision bzw. Dirty-Diff-Hash, Szenario-ID, Umgebung, Fenstergröße/DPI, Bildpfade, Aufnahmemethode, beobachteter Befund und Korrektur festhalten. `visually-reviewed`, `captured-not-reviewed`, `not-run` und `blocked-external` unterscheiden.

Offscreen-Bilder sind für Layout hilfreich, ersetzen aber keinen nativen Window-/GPU-/Wayland-Lauf. Wenn kein Bildleseweg verfügbar ist, weiterhin automatisierte Layout-/Funktionstests durchführen und die ausstehende optische Begutachtung ausdrücklich offenhalten. Nicht routinemäßig Screenshots vom Nutzer anfordern, die der Agent selbst erzeugen kann.

## 4. Zuordnung zu den bestehenden Paketen

| Paket | Ergänzter Pflichtumfang |
|---|---|
| A08 | Echte UI-Testbasis plus reproduzierbare Screenshotaufnahme und Nachweis der erfassten Darstellungsebenen |
| U03 / U11 | Genehmigte Gestaltung vergleichen; vollständige Abläufe selbst bedienen, Bilder betrachten, Abweichungen beheben |
| P01 / P04 | Verfügbare Host-/Automatisierungswege inventarisieren; native FreeBSD/Sway-Basis selbst prüfen; Meridian erst in L07 integrieren |
| P03 / L07 / L08 | DPI-/GL-/Compositorfälle selbst prüfen; nur nicht beobachtbare reale Hardwarewirkung als Rest an den Nutzer |
| L01 | Logs, UI-Bilder und visuelle Vergleichsergebnisse als versionsgebundene Prüfartefakte erfassen |
| L09 | Tutorials eigenständig gegen installiertes Produkt abarbeiten; Nutzer ist kein verpflichtender Testausführer |
| Alle betroffenen UI-Pakete | Abschnitt 3 als Bestandteil der Paketabnahme, auch ohne erneute Wiederholung im Tickettext |

Die ursprünglichen 110 Hauptpakete bleiben bestehen; Kapitel 11 ergänzt elf Hauptpakete für native Aktionen und spätere KI/FEM/Optimierung (121 insgesamt). Neue Testhilfen werden ihren Paketen als begrenzte Unteraufträge zugeordnet. Die einheitliche Regel steht zusätzlich als `executionPolicy` im Manifest; sie hebt keine Paketabhängigkeit auf.

## 5. Wann der Nutzer wirklich gebraucht wird

Erst nach den möglichen Eigenprüfungen wird der konkrete Rest vorgelegt:

- **Fehlende Umgebung/Zugang:** etwa tatsächlicher Meridian-Host oder nicht vorhandene GPU-/Displaykonstellation. Zuerst Build, Startskript, Referenzmodell und automatisierbare Prüfungen fertig vorbereiten.
- **Nicht beobachtbare physische Wirkung:** etwa sichtbares Flackern am realen Display oder Resume-Verhalten, das zugängliche Aufnahmen/Telemetrie nicht zuverlässig erfassen. Nur diesen Rest testen lassen.
- **Nicht ableitbare Produktpräferenz:** wenn bestehende genehmigte Gestaltung und Verträge eine wichtige Entscheidung offenlassen. Varianten konkret vorbereiten und eine Empfehlung begründen.
- **Tatsächlich erforderliche neue Autorisierung:** wenn eine Handlung außerhalb des beauftragten Umfangs oder eine geltende Berechtigungsgrenze dies verlangt. Prüfungen und reviewbares Ergebnis vorher fertigstellen; frühere Autorisierung nicht erneut abfragen.

Normale Codekorrekturen, Regressionsläufe, eigenes Betrachten von Bildern und kleine Layoutkorrekturen innerhalb der genehmigten Gestaltung benötigen keine Nutzerbestätigung. Subjektive Ergonomie kann der Nutzer freiwillig beurteilen; daraus entsteht kein Pflichtstopp an jedem Gate.

## 6. Minimales Resttest-Paket für den Nutzer

Wenn wirklich nötig, eine gebündelte kurze Anfrage mit:

1. Fertigem Build/Startbefehl und vorbereitetem Referenzmodell.
2. Bereits selbst bestandenen Checks und **genauer Grenze**, die weitere Eigenprüfung verhindert.
3. Nur den verbleibenden Schritten, möglichst wenigen Minuten Aufwand, mit sichtbarem Sollverhalten.
4. Einfacher Rückmeldung, beispielsweise „nach Resume korrekt / schwarz / flackert“; Logs soweit möglich automatisch erfassen.
5. Konkreter Angabe, welches Paket/Gate betroffen ist und welche unabhängige Arbeit weiterläuft.

Kein „Teste bitte die ganze App“. Eine ausbleibende Antwort zählt nicht als bestanden. Der Agent übernimmt anschließend Diagnose, Fix, Wiederholungsprüfung und Statuspflege selbst.
