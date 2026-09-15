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

Jetzt wird kein Release erzeugt und kein Tag gesetzt. D11 sowie die nativen
Store-/Autoritätspakete bleiben gemäß Implementierungsplan offen.
