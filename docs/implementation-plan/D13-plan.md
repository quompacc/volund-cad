# D13 – Native Produktpersistenz in Teilblöcken

Status: **in-progress**, 16.09.2026. D12 ist abgeschlossen.
Der Produktwriter bleibt bis zur zusammenhängenden Umschaltung VOLUND 16.

| Block | Umfang | Abschlussgrenze |
| --- | --- | --- |
| D13a | Privater BinOcaf-Payloadadapter mit Schema-/Identitätsvalidierung | [Nachweis](evidence/D13a.md): encode/decode, vier Featurearten, Fehlerfälle, beide Plattformen. Noch kein Produktdispatch. |
| D13b | Produktformat und Einlesen vorbereiten | Eindeutige Kennung ohne Decoder-Fallback, Anwendungsschema getrennt von Container 12; begrenzte Eingabe einschließlich nicht suchbarer Streams, Fehler und Ressourcenpolitik vor Produktfreigabe klären. |
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
