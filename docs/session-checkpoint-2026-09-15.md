# Tagesabschluss – 15. September 2026

## Verbindlicher Stand für die Fortsetzung

D10 ist auf Nutzerauftrag abgeschlossen. D11a–D11g sind geprüft; **D11 insgesamt
bleibt `in-progress`**. D12/D13 und Meridian-Abnahme L07 sind weiterhin offen.

D11f/g, ihre Abnahmebelege und diese Abschlussdokumentation werden gemeinsam auf
`main` eingecheckt. Prüfbasis war `a5d76d9`; den aktuellen Abschlusscommit mit
`git log -1 --oneline` feststellen. Frühere Uncommittet-Vermerke in historischen
Nachweisen beschreiben den Zustand während der Tests. Keine Terra-Übergabe,
kein Push, kein Release-Tag. Heute keine weitere Implementierung.

## Zuletzt umgesetzt und geprüft

- [D11f](implementation-plan/evidence/D11f.md): privater nativer Dokumentkopf mit
  Kennung, Schemaentwurf 1, DocumentId und 64-Bit-Feature-Vergabemarke.
- [D11g](implementation-plan/evidence/D11g.md): private native Box-Abbildung samt
  Körper-/Feature-IDs, Namen, Reihenfolge und neun Maß-/Placementwerten. Winkel
  bleiben Radiant. Andere Featurefamilien werden vor dem Schreiben abgewiesen.
- **60/60 Windows**, **56/56 FreeBSD/Sway**, **281 Quellhashes identisch**.
- Native Speicherzyklen in beide Richtungen über OCCT 8.0.1 und 7.9.3 bestanden:
  vollständige Snapshot-Gleichheit und erneut ausgewertete gültige BReps.
- Alle gestarteten Builds und Prüfprozesse beendet. Tests und Belege stehen unter
  `docs/implementation-plan/evidence/D11g-artifacts`.

Der produktive Writer bleibt **VOLUND 16**. Die Adapter bilden keine zweite
veränderliche Dokumentautorität. Produktive New-/Open-/Save-As-/Duplikat-Anbindung
und der vollständige native Store sind noch nicht umgesetzt.

## Nächster Block

Native **Zylinderfamilie** mit Eingabereferenzen und Join/Cut/Intersect/Separate.
Danach Skizzen, Constraints und Extrusionen. Jeweils Snapshot-Parität nachweisen,
bevor der Autoritäts-/Store-Wechsel gemäß [ADR 0008](adr/0008-ocaf-binary-persistence.md)
und D12/D13 den Produktdispatch öffnet. D11a–D11g nicht erneut beginnen.

## FreeBSD-Prüfumgebung

- Rechner: `eduard@192.168.1.238`.
- Isolierter Prüfbaum: `/home/eduard/src/voelund-p04-20260913`.
- OCCT-Prefix: `/home/eduard/src/voelund-p04-20260913/build/d11d-occt/install`.
- `OpenCASCADE_DIR`: `<Prefix>/lib/cmake/opencascade` ausdrücklich setzen.
- Achtung: `.tools/occt-freebsd` im Prüfbaum verweist auf die ursprüngliche
  gemeinsame Installation. Diese wurde nicht überschrieben. Weiter den isolierten
  D11d-Prefix nutzen und keine System-OCAF-Bibliotheken zumischen.
- D11f- und D11g-Transfers wurden nach automatischer Ablehnung jeweils ausdrücklich
  vom Nutzer freigegeben und anschließend erfolgreich ausgeführt.
- Die genauen Build-/Testbefehle stehen in den jeweiligen Evidenz-Prüfskripten.

## Versionierung

[Changelog](../CHANGELOG.md) unter `Unreleased` aktuell. Basisversion `0.1.0`;
[Versionsregeln](versioning.md) dokumentiert. Eine zentrale Build-Kennung mit
Vorab-Version und Git-SHA ist vor einem ersten verteilten Release noch anzubinden.

## Bisherige Commits

- `6737465`: D10-Abschluss und D11a–D11e mit Abnahmebelegen.
- `a5d76d9`: Dokumentationsabschluss, Changelog und Versionsregeln.
- Der nachfolgende Tagesabschluss enthält D11f/g samt Dokumentation und Belegen.
