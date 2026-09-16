# D12a – Private Dokumentautorität vorbereiten

Status: **implementiert**, 16.09.2026; Abnahme siehe [D12a-Nachweis](evidence/D12a.md).
Der folgende Plan bleibt als Umfangsdefinition erhalten. D12 ist `in-progress`;
der OCAF-Backendwechsel steht weiterhin aus.

## Ziel des ersten Blocks

Die bestehende Modellautorität hinter einer privaten Implementierung kapseln,
bevor ihre Speicherung auf OCAF wechselt. Damit lassen sich Lebensdauer, Kopie
und isolierte Transaktionskandidaten getrennt vom späteren Backendwechsel prüfen.
D12a darf die heutige Vektorautorität privat verschieben; es führt noch keinen
parallel beschreibbaren OCAF-Zustand ein. ADR 0008 und der D11-ID-Vertrag gelten.

## Konkreter Umfang

1. `ModelDocument`-Daten und History in eine private Implementierung unter
   `src/document` verschieben; öffentliche Abfragen und Befehle zunächst erhalten.
   Auch `DocumentTransaction` und Snapshot-/Legacy-Zugriffe auf Interna erfassen.
   Öffentliche Header dürfen weder TDF-Labels noch OCAF-Dokumenthandles zeigen.
2. Explizite Wertkopie mit unabhängigem Zustand, Move und Lebensdauer definieren.
   Ein kopierter oder vorbereiteter Kandidat darf das Original nicht verändern.
   Die bestehende DocumentId-Lebensdauer an CadSession/IdentifiedDocumentSnapshot
   erhalten; keine zweite ID-Vergabe beim Kopieren des Modellinhalts.
3. Die bestehende prepare/commit-Grenze erhalten: Allokation und Validierung vor
   Veröffentlichung, veraltete Kandidaten ablehnen, Rollback ohne Seiteneffekt.
   Rückgaben per Span/Referenz und ihre Gültigkeit bei Mutation bewusst erhalten
   oder sämtliche Aufrufer im selben Block anpassen; keine verdeckten Dangling-Views.
4. Feature-Vergabestand niemals durch Undo absenken. Revision, Dirty-Vergleich und
   History-Semantik bleiben getrennt. Keine Änderung an Produktcodec oder UI.

## Abnahme für D12a

- Bestehende Dokument-/Anwendungstransaktions-, Allokationsfehler-, Snapshot-,
  Identitäts- und Undo/Redo-Tests bestehen; gezielter Kopie-/Kandidaten-Nachweis
  belegt unabhängig veränderbare Zustände und unverändertes Original bei Fehlern.
- Vollständige Windows- und FreeBSD/Sway-Suiten auf denselben Quellen bestehen;
  600-Zeilen-Grenze und öffentliche Headergrenzen bleiben erfüllt.
- Ein Review zählt pro Modellzustand genau eine beschreibbare Autorität.
- Kein Wechsel des Writers und keine neue Behauptung zu nativer Produktpersistenz.

## Anschließender D12-Backendwechsel

Danach OCAF-Parameter/IDs/Referenzen zur einzigen Autorität machen und immutable
Snapshots für Auswertung und Anzeige bereitstellen. Vor Veröffentlichung alle vier
Featurefamilien gegen D11l abgleichen; keinen produktiven Doppelwriter als Übergang
betreiben. OCAF-Undo muss den monotonen Vergabestand erhalten, obwohl es Inhalte
zurücksetzt. Änderungen an prepare/commit benötigen erneute Fehler-/Allokations-
und D05/D06-Abnahme; ein bestandener Codec-Roundtrip genügt dafür nicht.

Körper dürfen direkt zum Dokument gehören. Eine OCAF-Gruppe ist interne
Organisation und darf keine Komponenten-/Baugruppenpflicht im Benutzerworkflow
begründen. Der produktive native Store und dauerhafte DocumentId beim Öffnen
folgen erst in D13. D12a allein erfüllt die D12-Gesamtabnahme noch nicht.
