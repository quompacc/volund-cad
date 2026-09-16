# D13b2 – Ausgabebegrenzung und aggregierte Ressourcenregeln

Status: **auf Windows und FreeBSD/Sway geprüft**.
Ausgangspunkt: `89f9ca2`. D13b2-Gesamtreview und D13 bleiben offen.

## Umsetzung

NativeDocumentStore schreibt jetzt über BoundedNativeOutput. Der seekbare
Puffer prüft jeden Block- und Einzelbytezugriff vor dem Vergrößern gegen die
64-MiB-Dateigrenze. Es gibt keinen direkten Stream-Put-Bereich, der diese
Prüfung umgehen könnte.

OCCT kann bereits geschriebene Größen-/Offsetfelder nachtragen. Seek darf sich
nur innerhalb der vorhandenen Bytes bewegen. Überlauf, negative/übergroße
Positionen und Zugriffe hinter das geschriebene Ende scheitern. Fehlgeschlagene
Schreibzugriffe veröffentlichen keine fertigen Dateibytes. Die bestehende
abschließende Größen- und native Inhaltsprüfung bleibt zusätzlich erhalten.

Die Grenze betrifft logische Dateibytes. String-Kapazität, native Dokumente,
Snapshots und sonstige Laufzeitstrukturen benötigen zusätzlichen Speicher.
Es handelt sich nicht um ein genaues RAM-Limit oder eine Laufzeitgarantie.

## Neue und erweiterte Tests

**Neu: document.native_output_limits**

- Exakt passendes Limit, Einzelbyte- und Blocküberlauf ohne Teilschreiben.
- Rückwärtskorrektur bestehender Bytes und korrektes Dateiende.
- Negative, zu große und extreme Seek-Positionen.
- Streamausnahmen, Null-Limit, leere Writes und Übertragung des Ergebnisbuffers.
- Echter BinOcaf-12-Writer mit exakt passender Kapazität und mit einem Byte weniger.
- Korrektes Decode der Ausgabe; fehlgeschlagene Ausgabe verändert das native
  Dokument nicht, ein anschließender gültiger Rundlauf besteht.

**Erweitert: document.native_attribute_values**

- Genau 100.000 Referenzen mit je zwei Tags erfüllen das dokumentweite
  200.000-Tag-Budget; eine zusätzliche Referenz wird zurückgewiesen.
- Interne Attribut-ID 200.000 wird akzeptiert, 200.001 abgewiesen.
- Diese synthetischen Grenzfixtures laufen nur durch die Vorprüfung; sie werden
  nicht als fachlich gültige Produktdokumente an OCCT weitergegeben.

Die bereits vorhandenen Prüfungen für Treiberzuordnung, Array-/Referenzwerte,
Header, Baumstruktur und Dateiende laufen unverändert mit.

## Gemeinsame Plattformabnahme

- **75/75 Windows**, 101,51 s.
- **72/72 FreeBSD/Sway**, 38,43 s.
- **317 Quellhashes identisch**, nach den Prüfläufen lokal unverändert.
- Die zuvor nur lokal geprüfte Attributimplementierung ist Bestandteil dieses
  gemeinsamen Builds und der vollständigen FreeBSD-Suite.
- Vorhandene D11l-Dateien windows-schema.cbf und freebsd-freebsd-schema.cbf
  bestehen unter Windows erneut natives Decode. Kein neuer Dateiaustauschlauf.
- Zehn Dateien samt Prüfskript ausdrücklich freigegeben und in den bestehenden
  isolierten FreeBSD-Prüfbaum übertragen; OCCT-Prefix build/d11d-occt/install.
- Alle Build-/Testprozesse beendet.

[Artefakte](D13b2-limits-artifacts): Builds, CTest-Logs/JUnit, gezielte Windows-
Prüfung, historische Austauschprüfung, Manifest/Hashvergleich, Transferliste
und ausgeführtes Prüfskript.

## Fortsetzung

Das nächste D13b2-Gesamtreview ordnet die vier Prüfebenen und ihre Grenzen zu:
Transport/Dateikopf, native Struktur, Attributwerte/Treiber und Ausgabe.
Fachliche Referenzprüfung erfolgt weiterhin im Codec; vorhandene logische
Ressourcenbudgets sind keine exakte Obergrenze für Prozessspeicher.

D13b2/D13b werden erst nach dieser Prüfung formal abgeschlossen.
Native Produktpersistenz, AtomicFileStore-Anbindung und dauerhafte Dokument-ID
beim Produkt-Open bleiben D13c; Gesamtdateiaustausch und Save-Abnahme D13d.
