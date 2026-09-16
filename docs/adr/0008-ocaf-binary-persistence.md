# ADR 0008 – OCAF-Dokumentautorität und native Binärpersistenz

Status: angenommen für die Umsetzung in D09–D13, 14.09.2026.
Entscheidungsnachweis: [D08](../implementation-plan/evidence/D08.md).
Umsetzungsstand 16.09.2026: Der produktive Dokumentkern ist mit
[D12](../implementation-plan/evidence/D12e.md) auf OCAF umgestellt.
Der Produktwriter bleibt VOLUND 16; native Binärpersistenz folgt in D13.

## Entscheidung

OCAF wird gemäß K08 als einzige veränderliche Dokumentautorität hinter
`ModelDocument` umgesetzt. Als künftiger Produktstore wird **native BinOcaf-
Persistenz mit expliziter OCAF-Schreibversion 12** gewählt. Der neutrale Snapshot-
Fallback ist nach dem erfolgreichen D08-Austausch nicht der gewählte neue Writer.
Immutable Snapshots bleiben Anwendungs-/Migrationsschnittstellen, keine zweite
parallel veränderliche Datenquelle.

`BinDrivers::DefineFormat` registriert die Standardtreiber direkt an
`TDocStd_Application`. Lesen/Schreiben erfolgt über die nativen seekbaren Stream-
Überladungen. Der Dateipfad-Writer hängt andernfalls `.cbf` an die angegebene
`.volund`-Endung an; dies wurde im Experiment tatsächlich beobachtet. Der Stream-
Writer überlässt Dateinamen, temporäre Datei und atomaren Ersatz der vorhandenen
Plattform-/Store-Grenze. D08 verwendet noch keinen produktiven AtomicFileStore.

OCAF-Speicherformat 12 und Völund-Anwendungsschema sind unterschiedliche Versionen.
`ChangeStorageFormatVersion(TDocStd_FormatVersion_VERSION_12)` wird explizit gesetzt;
ein späterer OCCT-Default darf das Dateiformat nicht stillschweigend ändern.
Das Anwendungsschema wird am Root gespeichert und vor Veröffentlichung validiert.
Der im Experiment benutzte Wert 1 ist nur ein Probeschema und reserviert keine neue
Produktformatnummer. D09/D13 entscheiden die neue Kennung auf dem dann aktuellen Stand.

## Belegte Reichweite

Der identische C++-Nachweis läuft gegen OCCT 8.0.1 auf Windows und die vollständige
FreeBSD-Paketinstallation OCCT 7.9.3_4. Beide lesen eigene und gegenseitige Dateien
einschließlich Rücklauf nach erneutem Schreiben durch die jeweils andere Version.
Geprüft werden Root-/Body-/Feature-Identitäten, Unicode-Name, Integer-/Real-/ASCII-
Attribute, Labelreferenz, gültiger analytischer Box-BRep, Volumen, Boundingbox,
Solid-/Face-/Edge-Anzahl sowie ein TNaming-Modify-Paar mit alter und neuer Form.
Änderung, Undo und Redo funktionieren vor dem Speichern und erneut nach dem Laden.

Die Undo-/Redo-Stacks selbst werden nicht als Datei-Historie übernommen: Ein frisch
geladenes Dokument besitzt im Nachweis keine Undo-Einträge. Anschließend ausgeführte
Transaktionen funktionieren. Parametrische Featurehistorie und TNaming-Daten sind
Dokumentinhalt; sie dürfen nicht mit dem flüchtigen Undo-Stack verwechselt werden.

Dieser Test beweist weder allgemeines Toponaming noch Split/Merge-Zuordnung,
beliebige benutzerdefinierte Attribute oder künftige OCCT-Versionen. R03–R06 prüfen
Operationsherkunft und Referenzauflösung gesondert. Ein NamedShape allein ist keine
verlässliche Benutzerreferenz auf eine Fläche.

## Öffentliche API und private Struktur

- Öffentliche `ModelDocument`-/Anwendungsheader enthalten keine `TDF_Label`,
  `TDocStd_Document` oder OCCT-Attribut-Handles. Eine private Implementierung besitzt
  das OCAF-Dokument und die Zuordnung zu den in D11 definierten Identitäten.
- Produktive Labelstruktur: privater Schema-/Identitätsroot, ein Bereich für
  Körper, darunter Feature-/Parameter-/Referenzdaten gemäß D11. Labeltags oder
  Einfügereihenfolge werden nicht als öffentliche Feature-/Flächenidentität benutzt.
  Die Probetags 10/20 und Proben-UUIDs sind keine Festlegung der produktiven Struktur.
- Abfragen liefern unveränderliche DTO-Snapshots. Mutationen erfolgen ausschließlich
  durch die bestehende Befehls-/Transaktionsgrenze. D12 muss Undo/Redo, Fehler und
  Abbruch gegen die vorhandenen Regressionen beweisen, bevor alte Mutatoren entfallen.
- Abgeleitete Auswertungen und Darstellung bleiben Caches mit Dokumentstand.
  Parametrische Eingaben bleiben führend. Spätere importierte BReps sind dagegen
  Quelldaten des Importfeatures. Keine Form ersetzt ihre editierbaren Parameter.
- VOLUND1–VOLUND16 bleiben über den Legacy-Importer lesbar. Quelldateien werden
  nicht automatisch überschrieben. Unbekannte zukünftige Schemata werden abgewiesen.

## Produktive Historie nach D12

Die produktive Historie hält vollständige, nach Veröffentlichung unveränderte OCAF-
Stände. Änderungen werden einschließlich Validierung und abgeleiteter Leseansicht
vorbereitet; der Commit tauscht den Besitzer. Dies erhält die bestehenden Fehler-
und Allokationsgarantien. Native OCAF-Undo-Deltas werden im Produkt nicht verwendet.
Speicher- und Laufzeitkosten vollständiger Stände sind bekannt; eine Abnahme großer
Modelle ist damit nicht verbunden. Die D08-Delta-Probe oben bleibt ein Experiment.

## Benötigte Module und Integration

Direkte Experiment-Links: `TKBin`, `TKCAF`, `TKPrim`.
OCAF-spezifische Laufzeitmodule: `TKCDF`, `TKLCAF`, `TKCAF`, `TKBinL`, `TKBin`.
Die übrigen geometrischen Abhängigkeiten werden aus den exportierten OCCT-Targets
übernommen; vollständige tatsächlich geladene Listen liegen in D08.
Keine XCAF-, XML-, Qt-, OpenGL- oder neue Serialisierungsbibliothek ist für diesen
OCAF-Nachweis erforderlich. FreeBSD-Systembibliotheken können eigene transitive
Abhängigkeiten besitzen; deren Liste wird nicht als schlanker Produktumfang ausgegeben.

[D11d](../implementation-plan/evidence/D11d.md) ergänzt den schlanken FreeBSD-Build
um `TKBin` und seinen automatisch aufgelösten OCAF-Unterbau. Der isolierte Neubau,
native Austauschproben und die Produktsuite sind geprüft; alle OCCT-Laufzeitmodule
stammen aus derselben neuen Installation. Der D08-System-Paketnachweis allein
ersetzt diesen Integrationsbuild nicht. Der produktive Autoritätswechsel ist
durch D12 abgeschlossen. Keine Vermischung von System-OCAF und projektlokalem Kernel.

Windows-Proben deployen die aufgelösten Nicht-System-DLLs direkt neben die EXE und
schlagen beim Build mit unaufgelösten Abhängigkeiten fehl. D08 wurde danach ohne
Entwickler-/vcpkg-PATH ausgeführt. Die produktive Laufzeitbereitstellung bleibt an
den finalen Produktgraphen gebunden.
