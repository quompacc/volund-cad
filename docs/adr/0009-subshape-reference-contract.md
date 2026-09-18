# ADR 0009 – Herkunftsbasierte Subshape-Referenzen

Status: angenommen als R02-Vertragsentscheidung, 17.09.2026.
Verantwortlich: Völund-Dokument-/Feature-Schicht.

## Kontext und Entscheidung

F06 beschreibt den stillen Wechsel von Flächen-/Kantenbezügen durch Normalen,
Abstand und Segmentnummern. K06 wird durch den
[SubshapeRef-Vertrag](../subshape-reference-contract.md) konkretisiert:
Dokument/Körper/Erzeuger, typisierte Topologie und semantischer Herkunftsschlüssel
sind die persistente Identität. Die Zielrevision und der aktuelle Produzent gehören
zum Auflösungskontext. Resultate sind Resolved, Missing oder Ambiguous.

Vollständig belegte eindeutige Nachfolger dürfen aufgelöst werden. Splits bleiben
mehrdeutig, Merges auf eine nachgewiesene Fläche dürfen eindeutig sein. Geometrische
Ähnlichkeit ist ausschließlich Plausibilisierung bzw. Reparaturvorschlag. Eine
bewusste Neuzuordnung wird als einzelne Undo-Transaktion gespeichert.

## Alternativen und Folgen

Explorerindex, Normalen-/Abstandstreffer, größtes Splitstück und alleinige
TNaming-Labelnummer sind als Identität verworfen. Sie können sich bei unverändertem
Benutzerbezug ändern. Der Vertrag erfordert explizite Operationsadapter in R03,
produktive Auflösung in R04/R05 sowie Schemaerweiterung und Reparatur in R06.
Stabile Regionszuordnung bleibt R09; mehrdeutige Übergänge werden bis dahin
reparaturbedürftig. Keine allgemeine automatische Naming-Garantie.

Ein isolierter C++20-Entwurf mit 26 symbolischen Fixtures macht die Regeln
prüfbar. Er ist kein Produktresolver und kein Nachweis tatsächlicher OCCT-History.
Das aktuelle Anwendungsschema 1 und der Produktcode werden durch R02 nicht geändert.
Prüfstand: [R02](../implementation-plan/evidence/R02.md).

Erweiterungsanlass: nicht abbildbare echte Historie in R03, stabile Regionen R09
oder neue Featurearten. Erweiterungen benötigen Gegenfixtures und bei persistenten
Änderungen einen geprüften Migrationspfad. Es gibt keine befristete Ausnahme von
den Identitätsregeln.
