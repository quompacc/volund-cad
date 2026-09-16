# D13 – Native Produktpersistenz in Teilblöcken

Status: **in-progress**, 16.09.2026. D12 ist abgeschlossen.
Der Produktwriter bleibt bis zur zusammenhängenden Umschaltung VOLUND 16.

| Block | Umfang | Abschlussgrenze |
| --- | --- | --- |
| D13a | Privater BinOcaf-Payloadadapter mit Schema-/Identitätsvalidierung | [Nachweis](evidence/D13a.md): encode/decode, vier Featurearten, Fehlerfälle, beide Plattformen. Noch kein Produktdispatch. |
| D13b1 | Begrenztes Einlesen und Formatzuordnung | [Auf beiden Plattformen geprüft](evidence/D13b1.md): vollständige Bytes ohne Seek, 64-MiB-Grenze, Legacy-Versionen und native Containerfamilie; noch kein Produktdispatch. |
| D13b2 | Native Containerprüfung und Ressourcenpolitik | [Dateikopf](evidence/D13b2.md) und [Strukturprüfung](evidence/D13b2-structure.md) vor OCCT-Open. [Attributprüfung](evidence/D13b2-attributes.md) und [Ausgabelimits](evidence/D13b2-limits.md) geprüft; Gesamtreview noch offen. D13b2/D13b nicht abgeschlossen. |
| D13c | Save/Open und AtomicFileStore geschlossen anbinden | Genau ein Produktwriter; Dokument-ID bei nativem Open erhalten, Legacy-Open neue ID; Save-As, Dirty-/Save-Revision und starke Fehlergarantien erhalten. |
| D13d | Austausch und Gesamtprüfung | Native Dateien Windows ↔ FreeBSD, A-SAVE-01/02, editierbare Folgefehler; D13 erst nach Gesamtabnahme abschließen. |

## Grenzen

- Die D13a-Schnittstelle verarbeitet reine BinOcaf-Payloads. Produktkennung,
  Dispatch und Kompatibilität mit früheren Experimentdateien sind noch kein
  freigegebener Produktvertrag. VOLUND 17 wird nicht beiläufig eingeführt.
- Containerformat 12 und Anwendungsschema 1 sind getrennte Versionsräume.
  Anwendungsversion 0.1.0 ist ein dritter.
- D13a begrenzt die Eingabelänge vor OCCT-Open auf 64 MiB. Das ist kein vollständiger
  Schutz gegen Allokationen durch manipulierte native Containerangaben. Encode
  prüft die Ergebnisgröße nach Serialisierung; noch kein begrenzter Ausgabepuffer.
  Ressourcenpolitik und Umgang mit angehängten Daten vor Produktfreigabe behandeln.
- Abgeleitete Geometrie und flüchtige Undo-Historie sind nicht die Parameterquelle.
  Nicht vorhandene künftige Suppression-/Importshape-Felder werden nicht erfunden.
- D14 prüft die vollständige Migration aller Altversionen. A-BODY-03 und Meridian
  L07 bleiben eigene Abnahmen.

## Präzisierung D13b

Der Eingabetransport und die native Containerprüfung sind getrennte Prüfgrenzen.
D13b1 liest vom aktuellen Streamstand bis EOF, begrenzt die transportierten Bytes
und wählt anhand der exakten Legacy-Kennung bzw. OCCT-Binärkennung eine Formatfamilie.
Es ruft keinen Decoder auf. Ein Binärpräfix bestätigt weder eine gültige Datei
noch eine unterstützte Container- oder Anwendungsversion.

D13b2 muss die verbleibenden Risiken nativer interner Größenangaben und angehängter
Daten konkret prüfen. Keine zusätzliche Eigenformat-Hülle oder VOLUND-17-Kennung
ist durch D13b1 beschlossen. Die anschließende Integration in D13c darf diese
offene Grenze nicht überspringen.

## D13b2 – Dateikopf und verbleibende Nutzdatenprüfung

Die erste Vorprüfung akzeptiert ausschließlich die BinOcaf-12-Kopfform der
unterstützten Little-Endian-Builds. Sie begrenzt Metadaten auf 1 MiB, einzelne
Textfelder auf 64 KiB und Listen auf 200.000 Einträge, zusätzlich begrenzt durch
die tatsächlich verfügbaren Bytes. Negative Längen, unerwartete FSD-Sektionen
und inkonsistente Grenzen werden vor OCCT-Open abgewiesen.

Die anschließende [Strukturprüfung](evidence/D13b2-structure.md) kontrolliert
Abschnittstabelle, äußere Attributblockgrößen, Baumtiefe/Labelanzahl und das exakte
Dateiende. Die nachfolgende Attributprüfung ergänzt innere Werte und Treiberzuordnung. Der FSD-Wert edata markiert hier nur das Metadatenende,
nicht das Dateiende. Eine bloße Prüfung edata == Dateigröße wäre fachlich falsch.
Die D13c-Produktumschaltung bleibt bis zur geklärten Nutzdaten-/Ressourcengrenze offen.

## D13b2 – Attributwerte und Treiberzuordnung

Die Attributprüfung bindet numerische Typ-IDs an die fünf erlaubten Namen aus
der Header-Treiberliste. String-, Integer-, Real-, RealArray- und Reference-
Nutzdaten werden vor OCCT-Open geprüft. Unbekannte/duplizierte Treiber,
zusätzliche Header-Referenzangaben, benutzerdefinierte GUID-Nutzdaten,
Delta-Arrays und direkt gestreamte Attribute gehören nicht zum aktuellen
Parameterschema und werden abgewiesen.

Die gespeicherten internen Attribut-IDs müssen eindeutig und höchstens 200.000
sein. Referenzpfade sind einzeln auf 256 Tags, insgesamt auf 200.000 Tags
begrenzt. Das begrenzt auch das Erzeugen zusätzlicher Labels durch den nativen
Referenztreiber. Fachliche Referenzziele bleiben anschließend Sache des Codecs.

Die Attributprüfung ist zusammen mit den [Ausgabelimits](evidence/D13b2-limits.md)
auf beiden Plattformen geprüft. Referenzbudget und maximale interne Attribut-ID
haben zusätzliche Grenztests. Vor D13b2-Abschluss folgt das Gesamtreview der
Ressourcenregeln; noch kein D13c-Writerwechsel.

## Ausgabepuffer

Der seekbare native Ausgabepuffer begrenzt ab diesem Stand jeden Schreibzugriff
vor dem Vergrößern auf höchstens 64 MiB logische Dateibytes. Rückwärtskorrekturen
von Größen/Offsets bleiben möglich; Seek außerhalb der bereits geschriebenen
Bytes scheitert. Die abschließende Größenkontrolle bleibt zusätzlich erhalten.

Dies ist keine Obergrenze des Prozessspeichers: String-Kapazität, OCAF-Zustand,
Snapshots und sonstige Laufzeitstrukturen benötigen zusätzlichen Speicher.
Die Produktintegration darf die Bytegrenze nicht als RAM-Garantie ausgeben.
