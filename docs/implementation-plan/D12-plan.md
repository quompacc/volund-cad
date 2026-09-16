# D12 – Teilblöcke und Abnahmegrenzen

Aufteilung vom 16.09.2026, vom Nutzer bestätigt. D12 bleibt bis zur Gesamtabnahme
`in-progress`. Der bisher als D12b bezeichnete vollständige Autoritätswechsel wird
in Vorbereitung, isolierten OCAF-Zustand, Produktumschaltung und Abnahme zerlegt.

| Block | Umfang | Abschlusskriterium |
| --- | --- | --- |
| D12a | Private Modellautorität, Wertkopie, Transaktionskandidaten | [Geprüft](evidence/D12a.md), 66/66 Windows, 62/62 FreeBSD. |
| D12b | Unveränderliche Snapshot-Abfragen; Auswertung/Writer an einen eingefrorenen Stand binden; eigene UI-Datenwerte | [Nachweis](evidence/D12b.md): keine Abhängigkeit dieser Ergebnisse von späteren Modellmutationen. |
| D12c | Privaten OCAF-Zustand für alle vier Featurearten isoliert entwickeln | [Nachweis](evidence/D12c.md): vorbereitete OCAF-Stände mit privater Historie; keine produktive zweite Autorität und keine native Delta-Undo-Abnahme. |
| D12d | Produktmodell und Transaktionen/Undo/Redo geschlossen auf OCAF umstellen | [Nachweis](evidence/D12d.md): OCAF-Autorität mit unveränderlicher abgeleiteter Leseansicht, D05/D06-Regressionen und produktiver Identität. |
| D12e | D12-Gesamtabnahme und Bereinigung | Fehlerfälle, ID-Erhalt und beide Plattformen vollständig; alte Modellautorität entfernt, keine Doppelwriter. |

## D12c – geprüfter Umfang

Implementierung und Grenzen stehen im [D12c-Nachweis](evidence/D12c.md).
Die folgende Umfangsdefinition bleibt als Grundlage erhalten.

- Bestehenden privaten nativen Adapter als Schema-Grundlage nutzen; keine zweite
  Kodierung der vier Featurefamilien einführen.
- OCAF-Zustand ausschließlich in privaten Dokumentquellen kapseln. Aus unveränderlichem
  Snapshot initialisieren und in semantisch gleiche Snapshots zurücklesen.
- Änderungen einschließlich zusammengesetzter Änderung und Abbruch isoliert prüfen:
  fehlgeschlagene Vorbereitung/Validierung veröffentlicht nichts, erfolgreicher
  Commit veröffentlicht genau einen neuen Stand. Nichtwiederverwendung von IDs erhalten.
- Vor dem Produktwechsel klären und belegen, wie OCAF-Historie, monotoner Vergabestand
  und vorbereiteter Commit die bestehenden Fehler-/Allokationsgarantien erfüllen.
  Keine stillschweigende Lockerung dieser Garantien.
- Testzustand darf neben Produktcode existieren, wird aber nicht als zweiter
  beschreibbarer Spiegel des laufenden Produktdokuments geführt.

## D12d – Produktintegration

D12d übernimmt zusammenhängend Modellautorität und Transaktionsanbindung; siehe [Implementierung und Nachweise](evidence/D12d.md).
Der D12c-Prototyp hält vollständige OCAF-Stände statt nativer Undo-Deltas;
diese Entscheidung und ihre Speicher-/Laufzeitkosten bei der Integration
explizit erhalten oder einen anderen Mechanismus gleichwertig nachweisen.
Wenn die D12c-Garantien nicht nachweisbar sind, wird diese Umschaltung nicht vorgezogen.
Der Produktwriter bleibt während D12 VOLUND 16; native Produktpersistenz folgt in D13.
Mehrkörper ohne Komponenten-/Baugruppenpflicht bleibt verbindlich. A-BODY-03 ist
weiterhin eine eigene offene Bedienabnahme; D12 erfüllt sie nicht durch Codec-Tests.

## Nächster Block: D12e

Gesamtabnahme gegen die ursprünglichen D12-Kriterien. Verbliebene interne Zugriffe,
öffentliche Header und Schreibwege prüfen; native Attribute und abgeleitete
Leseansichten dürfen nicht auseinanderlaufen. Bestehende D05/D06-Fehler-/History-
Nachweise zuordnen und Altbehauptungen zur Vektorautorität berichtigen. D12 erst
nach dieser Prüfung abschließen. D13 bleibt das eigene Produktstore-Paket.