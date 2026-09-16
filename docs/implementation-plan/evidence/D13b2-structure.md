# D13b2 – Zweiter Teil: native Containerstruktur

Status: **Strukturprüfung auf Windows und FreeBSD/Sway geprüft**.
Ausgangspunkt: `012814d`. D13b2 und D13 bleiben insgesamt offen.

## Umfang

NativeContainerStructure ergänzt die [Dateikopf-Vorprüfung](D13b2.md).
NativeDocumentStore prüft nun beide Ebenen vor OCCT-Open und kontrolliert auch
seine erzeugten Bytes vor Rückgabe. Die Produkt-API bleibt am bisherigen Writer.

Der Prüfer verwendet string_view, feste Zähler und begrenzte Rekursion. Er
reserviert keine Puffer aus gespeicherten Größenangaben. Ausnahmen dürfen wie
bisher Diagnosespeicher benötigen.

Geprüft wird die BinOcaf-12-Struktur unserer aktuellen Parameterdokumente:

- Eine Endsektion mit Name ":" und leeren Offset-/Längen-/Postread-Feldern.
  Zusätzliche Shape-/Anwendungssektionen gehören noch nicht zum unterstützten Schema.
- Root-Tag 0; positive Kind-Tags; eindeutige Endmarker für Attributlisten und Labels.
- Jeder gespeicherte Teilbaum muss in seinen Elternbereich passen und exakt
  an seiner angegebenen Grenze enden. Größen enthalten ihr eigenes 8-Byte-Feld.
- Attributblöcke benötigen positive Typ-/Objekt-IDs und eine innerhalb des Labels
  liegende Länge. Negative IDs für direkt gestreamte Attribute werden abgewiesen,
  da unser Parameterschema diese nicht verwendet.
- Höchstens 256 Label-Ebenen einschließlich Root, 200.000 Labels und 200.000
  Attributblöcke. Die bestehenden 64-MiB-Datei- und Metadatenlimits gelten zusätzlich.
- Hinter dem vollständigen Root-Teilbaum muss EOF liegen. Angehängte Bytes und
  verkettete Dokumente werden vor OCCT-Open abgewiesen.

Die Prüfung nutzt Subtraktionsvergleiche vor der Addition von Längen und hält
jeden Zugriff innerhalb des bereits validierten Elternbereichs. Sie interpretiert
keine Formgeometrie und führt keinen Attributtreiber aus.

## Quellenbasis

Der Aufbau wurde gegen die lokalen OCCT-8.0.1-Quellen geprüft:
BinLDrivers_DocumentSection (TOC), DocumentStorageDriver/DocumentRetrievalDriver
(Labelgrößen und Traversierung), BinLDrivers_Marker und BinObjMgt_Persistent
(äußere Attributdatensätze). [Quellhashes](D13b2-structure-artifacts/occt-source-review-hashes.txt).
Laufzeitkompatibilität ist mit Windows/OCCT 8.0.1 und FreeBSD/OCCT 7.9.3 geprüft.

## Neuer Testumfang

**document.native_container_structure** ist ein zusätzlicher CTest-Eintrag:

- Gültiges natives Dokument mit unverändertem Snapshot nach Encode/Decode.
- Abschneiden an jeder Position der Nutzdaten; fremde Abschnittsgröße/-kennung,
  große Offset-/Längenwerte, falscher Root-Tag und Endmarker.
- Teilbaum außerhalb der Elternbegrenzung und überlaufende Attributblockgröße.
- Ungültige Typ-/Objekt-ID und nicht unterstützte direkt gestreamte Attribute.
- Exakte Tiefen-, Labelanzahl- und Attributanzahlgrenzen sowie jeweils ein
  zusätzlicher Eintrag. Synthetische Grenzfixtures prüfen nur die Struktur,
  nicht gültige fachliche Völund-Dokumente, und werden nicht an OCCT weitergereicht.
- Angehängte Bytes und verkettete Dateien; NativeDocumentStore weist zusätzliche
  Bytes ausdrücklich schon durch die Strukturprüfung ab.
- Offene Grenze im Test festgehalten: Ein äußerlich gültiger Attributblock kann
  noch ungültige innere Werte enthalten. Diese Prüfung behauptet keine Semantikprüfung.

## Abnahme

- **73/73 Windows**, 105,47 s.
- **70/70 FreeBSD/Sway**, 38,23 s.
- **312 Quellhashes identisch** und nach den Prüfläufen lokal unverändert.
- Vorhandene D11l-Austauschdateien windows-schema.cbf und
  freebsd-freebsd-schema.cbf bestehen unter Windows zusätzlich Strukturprüfung
  und natives Decode. Kein neuer Dateiaustauschlauf behauptet.
- Sechs Quellen samt Prüfskript ausdrücklich freigegeben und übertragen.
  FreeBSD baut weiterhin mit dem isolierten Prefix build/d11d-occt/install.
- Alle Prüfprozesse beendet.

[Artefakte](D13b2-structure-artifacts): vollständige Builds/CTest-Logs/JUnit,
gezielte Windows-Prüfung, historische Austauschprüfung, Manifest/Hashvergleich,
Transferliste, Skript und Hashes der untersuchten OCCT-Quellen.

## Zum damaligen Prüfstand offene Grenze

Die innere Attributstruktur und ihre Zuordnung zu den im Header genannten
OCCT-Treibern bleiben offen. Ein in die Datei passender Attributblock kann
beispielsweise eine unzulässige Array-Grenze enthalten, aus der sein Treiber
eine übergroße Reservierung ableitet. Typ-/Objekt-IDs werden bisher nur auf
ihren positiven Wertebereich geprüft, nicht auf vollständige semantische
Konsistenz. Zusätzliche Sektionen und direkt gestreamte Attribute sind bewusst
nicht Teil des aktuellen Formats.

D13b2 nicht abschließen, bevor diese Grenze geklärt und geprüft ist.
Die Ausgabegrößenprüfung nach Serialisierung bleibt ebenfalls bestehen.
D13c/Produktumschaltung folgt erst nach dem Ressourcenvertrag.

## Fortsetzung

Die [Attributprüfung](D13b2-attributes.md) ergänzt innere Werte und
Treiberzuordnung. Ihr eigener Plattformstatus ist maßgeblich; die obigen
73/70-Testergebnisse belegen ausschließlich den Strukturstand.
