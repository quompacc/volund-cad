# Versionierung und Releases

## Jetzt beginnen

Die Basisversion in `CMakeLists.txt` ist `0.1.0`. Sie bezeichnet den bisherigen
Entwicklungsstand, keinen bereits veröffentlichten Release. Wir führen ab jetzt
ein [Changelog](../CHANGELOG.md) und dokumentieren Änderungen unter `Unreleased`.
Ein Commit benötigt keine neue Produktversionsnummer. D10, D11 usw. bleiben
Arbeitspakete und sind keine Releases.

Die Nummerierung orientiert sich an [Semantic Versioning 2.0.0](https://semver.org/spec/v2.0.0.html).
`0.x` kennzeichnet die frühe Entwicklung ohne stabile öffentliche Schnittstelle.
Für Völund gilt in dieser Phase: neue Funktionspakete und inkompatible Änderungen
erhöhen die Minor-Version; reine kompatible Fehlerkorrekturen die Patch-Version.
Kompatibilitätsänderungen werden zusätzlich ausdrücklich im Changelog beschrieben.
Ab `1.0.0` müssen die zugesicherten öffentlichen Schnittstellen benannt sein;
inkompatible Änderungen daran erhöhen dann die Major-Version.

## Teststände eindeutig benennen

- Gezielte Vorab-Releases erhalten beispielsweise `0.1.0-alpha.1` und den
  annotierten Git-Tag `v0.1.0-alpha.1`. Das ist ein Beispiel, kein bereits gesetzter Tag.
- Normale Entwicklungsstände werden durch Commit-SHA und gegebenenfalls den
  Zusatz `dirty` identifiziert. Ein unveröffentlichter Build darf nicht allein
  über die wiederverwendete Basisnummer als eindeutig gelten.
- Ein freigegebener Tag wird nicht verschoben. Nachträgliche Änderungen erhalten
  eine neue Version. Die Changelog-Sektion erhält Versionsnummer und Releasedatum.
- Es gibt keine automatische Versionssteigerung je Commit oder je D-Paket.

## Produkt-, Datei- und Dokumentversion sind getrennt

| Kennung | Bedeutung |
| --- | --- |
| Produktversion, z. B. `0.1.0-alpha.1` | freigegebener Softwarestand |
| Git-Commit | genaue Quellrevision |
| VOLUND 16 | aktuelles geschriebenes Produktdateiformat |
| OCAF-Speicherformat 12 | native Container-Schreibversion aus ADR 0008 |
| künftiges natives Anwendungsschema | Struktur und Bedeutung der Völund-Daten im Container |
| DocumentId | Identität eines Dokuments über seine Revisionen hinweg |

Eine neue Produktversion erhöht keine dieser anderen Kennungen automatisch.
Lesekompatibilität und Migrationen werden pro Dateiformat dokumentiert.

## Vor dem ersten versionierten Build

1. Umfang und Zielplattform festlegen; offene Einschränkungen im Changelog nennen.
2. CMake-Basisversion als zentrale Quelle beibehalten und die derzeit noch
   fest codierte Qt-Anwendungsversion in `apps/voelund-cad/main.cpp` daraus ableiten.
   Vorab-Kennung, Commit-SHA und Dirty-Status in Build-/Diagnoseinformationen ergänzen.
   Diese Build-Integration ist noch offen; die vorliegende Änderung betrifft die Doku.
3. Den konkreten Release-Stand auf den angegebenen Plattformen bauen und prüfen.
   FreeBSD/Sway ist kein Ersatz für die noch offene Meridian-Abnahme.
4. `Unreleased` in eine datierte Release-Sektion überführen, neuen leeren Abschnitt
   anlegen, committen und den geprüften Commit annotiert taggen.
5. Falls Artefakte verteilt werden, Plattform, Abhängigkeiten, Prüfergebnisse und
   Prüfsummen beilegen. Veröffentlichung und Push erfolgen als eigener Auftrag.

D11 und D12 sind abgeschlossen. D13 bleibt gemäß Implementierungsplan offen;
D13b ist abgenommen, D13c ist der nächste Integrationsblock. Noch kein Release-Tag.

## Gitea-Repository vorbereiten

Das bestehende Gitea-Repository `volund` ist laut Nutzer für CAD Vault bestimmt.
Dieses CAD-Projekt erhält ein separates Repository. Namensvorschlag: **volund-cad**;
Produktname Völund CAD, C++-Namespace und lokales Projektverzeichnis bleiben erhalten.
Der Vorschlag ist noch nicht als endgültiger Remote-Name bestätigt.

Ein erster Push ist bereits während der Entwicklung sinnvoll: Er sichert die
Git-Historie auf einem weiteren Rechner und schafft einen gemeinsamen Stand für
spätere Zusammenarbeit und CI. Er ist kein Release und ersetzt keine unabhängige
Sicherung des Gitea-Servers.

Stand 16.09.2026: lokal kein Git-Remote konfiguriert. Vor dem ersten Push:

1. Zielname und Gitea-Clone-URL festlegen; vorzugsweise ein privates, leeres
   Repository ohne automatisch angelegte README oder Lizenz verwenden.
2. Getrackte Dateien und Historie auf Zugangsdaten, interne Angaben und große
   Build-/Binärartefakte prüfen; auch vorhandene Prüfprotokolle berücksichtigen.
3. Ziel als Remote eintragen und `main` mit seiner bestehenden Historie übertragen.
   Nicht in das bestehende CAD-Vault-Repository pushen.

Die Prüfung vor Veröffentlichung und der erste Push stehen noch aus.
