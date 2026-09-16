# D13b – Gesamtabnahme der Ein-/Ausgabegrenzen

Status: **D13b1, D13b2 und damit D13b abgeschlossen**, 16.09.2026.
Geprüfter Implementierungsstand: `62c9023`. D13 bleibt **in-progress**.
Dieser Abschluss ändert ausschließlich Dokumentation.

## Abnahmematrix

| Prüfgrenze | Befund und Nachweis |
| --- | --- |
| Transport und Formatfamilie | DocumentFileInput liest ohne Seek bis EOF, höchstens 64 MiB plus ein Überlauf-Prüfbyte. I/O-Fehler liefern keinen Teilwert. Exakte VOLUND-Versionen 1–16 und BINFILE werden getrennt; native Fehler fallen nicht auf Legacy zurück. `document.file_input`, [D13b1](D13b1.md). |
| Dateikopf | NativeContainerHeader prüft BinOcaf 12, Little Endian, Abschnittsgrenzen, Objektzahl, Textlängen und Listen vor OCCT-Open. Metadaten höchstens 1 MiB, einzelne Kopftexte 64 KiB. `document.native_container_header`, [Dateikopf](D13b2.md). |
| Struktur | Abschnittstabelle, Blocklängen, positive Tags, Tiefe höchstens 256, jeweils höchstens 200.000 Labels und Attribute sowie exaktes EOF. Additionen erfolgen erst nach Restlängenprüfung. `document.native_container_structure`, [Struktur](D13b2-structure.md). |
| Treiber und Werte | Header-Typnummern sind an fünf eindeutige erlaubte Treibernamen gebunden. Wertegrößen, endliche Reals, Arraygrenzen, eindeutige interne Attribut-IDs und Referenzpfade werden geprüft. Referenzen höchstens 256 Tags einzeln und 200.000 insgesamt. `document.native_attribute_values`, [Attribute](D13b2-attributes.md). |
| Ausgabe | BoundedNativeOutput prüft Einzelbyte- und Blockwrites vor Wachstum auf höchstens 64 MiB; Seek nur innerhalb vorhandener Bytes. OCCT darf Größen nachtragen. Echter Writer besteht mit exakt passendem Limit und scheitert bei einem Byte weniger. `document.native_output_limits`, [Limits](D13b2-limits.md). |
| Reihenfolge und fachliche Validierung | NativeDocumentStore::decode ruft die vollständige Struktur-/Attributprüfung vor NativeSession und OCCT-Open auf. Danach prüft NativePrimitiveCodec Kennung, Schema, IDs, Featureparameter, Reihenfolge und fachliche Referenzziele, bevor ein Snapshot zurückkehrt. Encode prüft seine fertigen Bytes ebenfalls vollständig. `document.native_store` und bestehende Schema-/Codec-Tests, [D13a](D13a.md). |

Framing-Prüfungen ohne Attributvalidierung dienen isolierten Tests. Der Store
aktiviert die Attributvalidierung beim Lesen und Schreiben ausdrücklich.
Kein teilweise geladenes ModelDocument wird durch diesen privaten Adapter veröffentlicht.

## Verifizierter Prüfstand

- Vorhandene vollständige Suite: **75/75 Windows**, 101,51 s;
  **72/72 FreeBSD/Sway**, 38,43 s. Beide JUnit-Dateien: null Fehler.
- Alle **317 lokalen Quellhashes** erneut gegen das gemeinsame Manifest geprüft:
  unverändert gegenüber der beidseitigen Abnahme.
- Buildlogs, JUnit und plattformübergreifender Hashnachweis liegen in
  [D13b2-limits-artifacts](D13b2-limits-artifacts).
- In diesem Dokumentationsblock keine neuen Builds, Testläufe oder Übertragungen.
  Die Zahlen bezeichnen die vorhandene Gesamtsuite, keine neu ausgeführten Tests.
- Der letzte Block prüfte vorhandene D11l-Austauschdateien erneut unter Windows.
  Ein neuer produktiver Dateiaustausch gehört zu D13d.

## Bewusste Grenzen

Der Vertrag gilt für die aktuelle Parameterstruktur, Containerformat 12 und die
geprüften Little-Endian-Builds mit OCCT 8.0.1/7.9.3. Unbekannte Treiber,
zusätzliche Sektionen, direkte Attribute, externe Referenzen, Custom-GUID-Werte
und Delta-Arrays werden abgewiesen. Das ist kein universeller BinOcaf-Importer.
Die festen Headerzähler entsprechen dem frisch erzeugten isolierten Writer-Dokument.

Die Grenzen beschränken logische Bytes und Anzahlen; sie sind keine exakte
RAM- oder Laufzeitgarantie. OCAF, Snapshots und String-Kapazitäten benötigen
zusätzlichen Speicher. Referenzpfade können vor der fachlichen Prüfung weitere
Labels erzeugen; auch ihre gesamte Taganzahl ist begrenzt. Ihre fachlichen Ziele
prüft der Codec nach dem nativen Öffnen.

DocumentSerializer liest weiterhin Legacy und schreibt **VOLUND 16**.
CadSession erzeugt beim bisherigen Legacy-Open eine neue Dokument-ID. D13b
schließt private Prüfgrenzen ab, nicht native Produktpersistenz oder A-SAVE-01/02.
A-BODY-03, Meridian L07 und die vollständige Altformatmigration D14 bleiben offen.

## Nächster Block: D13c

Der [Teilplan](../D13-plan.md) beschreibt die zusammenhängende Produktumschaltung:
identifizierter Importwert, eindeutiger Dispatch, genau ein nativer Writer und
CadSession/AtomicFileStore mit unveränderten Fehlergarantien. D13d prüft danach
Produktdateiaustausch und die ursprünglichen Save/Open-Abnahmen.
