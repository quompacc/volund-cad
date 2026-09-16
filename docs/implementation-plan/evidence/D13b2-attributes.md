# D13b2 – Dritter Teil: Attributwerte und Treiberzuordnung

Status: **auf beiden Plattformen geprüft** im [gemeinsamen Folgestand](D13b2-limits.md).
D13b2-Gesamtreview bleibt offen.
Ausgangspunkt: `ce26f6d`. D13b2 und D13 bleiben insgesamt offen.

## Umsetzung

Die Header-Treiberliste wird vor OCCT-Open geprüft. Numerische Typ-IDs werden
in der tatsächlich gespeicherten Reihenfolge auf genau fünf erlaubte Namen
abgebildet: TDataStd_AsciiString, TDataStd_Integer, TDataStd_RealArray,
TDF_Reference und TDataStd_Real. Unbekannte und doppelte Namen werden abgewiesen.

Der aktuelle isolierte Writer schreibt FILE_FORMAT: BinOcaf,
REFERENCE_COUNTER: 0 und MODIFICATION_COUNTER: 1 sowie genau eine
START_TYPES/END_TYPES-Liste. Die Vorprüfung verlangt diese Kopfform.
Zusätzliche Referenz-/Erweiterungsangaben werden nicht an OCCT weitergereicht.
Das ist ein Vertrag für unsere Parameterdateien, keine allgemeine BinOcaf-Unterstützung.

NativeDocumentStore aktiviert jetzt die zusätzliche Attributprüfung für Encode
und Decode. Der bisherige reine Strukturmodus bleibt für isolierte Grenztests
verfügbar. Die Vorprüfung benötigt keine aus Dateiangaben dimensionierten Puffer:
Treiberliste mit fünf Einträgen und ein festes Bitset für interne Attribut-IDs.

## Werte- und Ressourcenregeln

| Typ/Bereich | Vorprüfung |
| --- | --- |
| AsciiString | Genau ein abschließendes NUL, kein eingebettetes NUL, höchstens 4096 Nutzbytes. |
| Integer | Genau vier Bytes; fachlicher Wertebereich bleibt Sache des Codecs. |
| Real | Genau acht Bytes, endlicher Wert. |
| RealArray | Untergrenze 1, positive Obergrenze bis 1.000.000, alle deklarierten Werte im Block vorhanden, exakte Blockgröße, endliche Werte, Delta-Flag 0. |
| Reference | 1–256 Tags, exakte Blockgröße, Root-Tag 0 und danach positive Tags im int-Bereich. |
| Treiber-ID | Muss auf einen Eintrag der geprüften Liste zeigen. |
| Interne Attribut-ID | Positiv, höchstens 200.000, dokumentweit eindeutig. Keine fachliche Feature-ID. |
| Referenzbudget | Summe aller Referenzpfad-Tags höchstens 200.000. |

Die genaue Nutzdatengröße schließt optionale benutzerdefinierte GUID-Anhänge aus,
die unser Codec nicht schreibt. Direkt gestreamte Attribute und zusätzliche
native Sektionen bleiben ausgeschlossen.

OCCTs RealArray-Treiber reserviert das Array vor dem Einlesen seiner Werte.
Die Vorprüfung kontrolliert deshalb Grenzen und verfügbare Bytes bereits vorher.
Der Reference-Treiber kann fehlende Labels erzeugen; das zusätzliche Tagbudget
begrenzt diese Arbeit. Es beweist noch nicht, dass jedes Ziel schon im Baum
existiert. Fachliche Referenzen werden anschließend vom vorhandenen Codec geprüft.

Diese logischen Grenzen sind kein exaktes Prozess-RAM- oder Laufzeitlimit.
Es gelten zusätzlich die bereits geprüften Datei-, Metadaten-, Baum- und
Attributanzahlgrenzen. Die Ausgabegröße wird weiterhin nach Serialisierung geprüft.

## Quellen und Tests

Die lokale OCCT-8.0.1-Prüfung umfasst die fünf Treiber, BinObjMgt_Persistent und
BinLDrivers_DocumentRetrievalDriver. [Quellhashes](D13b2-attributes-artifacts/occt-source-review-hashes.txt).

Der neue CTest **document.native_attribute_values** prüft:

- Direkte gültige/ungültige String-, Integer-, Real-, Array- und Referenzwerte.
- Überlaufende, negative, fehlende und zu große Array-/Referenzangaben,
  nicht endliche Realwerte sowie unerwartete Zusatzbytes.
- Modellrundlauf über alle fünf Treiber mit unverändertem Snapshot.
- Unbekannte und doppelte Treiber, Reihenfolge der numerischen Treiberzuordnung,
  veränderten Referenzcounter, doppelte Attribut-IDs und unbekannte Typ-IDs.
- Manipulierte innere Array-/Referenzdaten über NativeDocumentStore::decode.

Die Gesamtabnahme muss die aggregierten Ressourcenregeln und den noch
nachgelagerten fachlichen Referenzcheck ausdrücklich einordnen. Nicht jeder
denkbare Grenzwert ist mit diesem einen Test vollständig abgedeckt.

## Historischer lokaler Prüfstand

**74/74 Windows** bestanden, 101,52 s; **315 lokale Quellhashes unverändert**.
Die vorhandenen D11l-Dateien windows-schema.cbf und freebsd-freebsd-schema.cbf
bestehen unter Windows zusätzlich natives Decode mit aktivierter Attributprüfung.
Kein neuer Dateiaustauschlauf und keine neue FreeBSD-Abnahme behauptet.
Die 70/70 FreeBSD aus dem Strukturteil gelten nur für dessen vorherigen Stand.

[Artefakte](D13b2-attributes-artifacts): Windows-Build und Gesamtsuite/JUnit,
historische Austauschprüfung, Quellmanifest, lokaler Hashnachweis und OCCT-
Quellprüfliste. Transferliste und Skript sind vorbereitet, noch nicht ausgeführt.
Alle lokalen Prüfprozesse sind beendet.
Die Freigabe für acht Dateien samt Prüfskript auf FreeBSD wurde angefragt;
noch keine Übertragung und kein neuer FreeBSD-Prüflauf.

Nächster Schritt: FreeBSD-Abnahme dieses Standes, dann D13b2-Gesamtreview
einschließlich offener Ressourcen- und Ausgabegrenzen. Produktumschaltung erst D13c.

## Nachgeholte FreeBSD-Abnahme

Der [Folgeblock mit Ausgabelimits](D13b2-limits.md) enthält diesen Attributstand
und zusätzliche Tests des Referenzbudgets sowie der maximalen Attribut-ID.
Gemeinsame Abnahme: 75/75 Windows, 72/72 FreeBSD/Sway, 317 identische Quellhashes.
Die zehn gemeinsamen Dateien samt Prüfskript wurden ausdrücklich freigegeben
und übertragen. Die obige offene Transfernotiz beschreibt nur den Zwischenstand.

## Nachtrag: Gesamtabnahme

Das [Gesamtreview vom 16.09.2026](D13b-review.md) schließt D13b2/D13b ab.
Die oben beschriebenen Prüfläufe bleiben der historische Nachweis. D13 bleibt offen.
