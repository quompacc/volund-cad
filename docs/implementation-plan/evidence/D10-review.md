# D10 – Abschlussprüfung vom 15. September 2026

**Historisches Reviewergebnis; der nachgewiesene ID-Überlauf ist inzwischen in [D10e](D10e.md) behoben.** D10 bleibt auf Nutzerauftrag `in-progress`.
Die numerischen D10c/D10d-Fälle bestehen. Die explizite A-NUM-01-Anforderung
„ID-Überlauf vor Mutation abweisen“ ist im Skizzeneditor noch nicht erfüllt.
D11 wurde nicht begonnen; HEAD bleibt `e8b65b8`, kein Commit oder Push.

## In D10e behobener Befund: ID-Überlauf bei Rechteckbestätigung (P1)

`SketchEditorRectangle.cpp:15–19` reserviert vier Element-IDs durch ungeprüftes
Postinkrement. Ein von der Dokumentvalidierung akzeptierter Entwurf mit
`elements.nextId = UINT64_MAX - 1` genügt. Die Vorschau eines Rechtecks wird
akzeptiert; `confirmRectangle` schreibt anschließend IDs bis einschließlich
null und eins in den Entwurf. Es ignoriert das fehlgeschlagene Solverergebnis
und gibt trotzdem eine ID als erfolgreiche Bestätigung zurück.

Der [isolierte Windows-Reproducer](D10-review-artifacts/IdOverflowProbe.cpp)
wurde gegen dieselben gebauten Produktbibliotheken wie die Tests gelinkt.
Er verändert keine Produktdatei und keinen bestehenden Test-Binary.
[Ausgabe](D10-review-artifacts/windows/id-overflow-probe.log):

```text
preview=1 confirmed=1 changed=1 zero_id=1 next_id=2
```

Exit 0 dieses **Diagnoseprogramms bedeutet Reproduktion des Fehlers**, keinen
bestandenen Abnahmetest. Das Verhalten widerspricht A-NUM-01 und der
Nichtwiederverwendung von IDs. Der konkrete dynamische Nachweis stammt von
Windows; es ist derselbe plattformunabhängige unsigned-Arithmetikpfad.

**Nächster begrenzter Auftrag:** Element- und Constraint-ID-Bedarf von
Editoroperationen vor Mutation prüfen, einschließlich Operationen mit mehreren
IDs (Rechteck, Slot, Polygon, Muster und Relationen). Erschöpfte Zähler dürfen
weder Entwurf, Auswahl noch Historie verändern. Dokument-/Import-Parität der
Zählergrenzen prüfen und negative Grenztests ergänzen. Keine D11-Identitätsmigration
für diese Korrektur voraussetzen. Erst danach A-NUM-01 erneut insgesamt bewerten.

## Gefunden und korrigiert: Support-Zusatzdaten an X/Y-Abständen

Die D10c-Erweiterung der Bereichsprüfung hatte versehentlich auch die
`supportCoordinate`-Klassifikation auf `pointDistanceX/Y` erweitert. Dadurch
akzeptierten diese Constraints zwei Zusatzwerte, die nur an echten
`coordinateX/Y`-Supportreferenzen zulässig sind.

Die Klassifikation ist korrigiert. Der neue Fall in `document.numeric_policy`
scheiterte vor der Korrektur und besteht danach: beide Richtungsarten werden
mit Zusatzdaten abgewiesen, Parameter und Dokumentrevision bleiben erhalten.
Der Legacy-Import verwendet denselben Constraintvalidator. Bestehende Tests
für gültige Supportreferenzen bleiben Teil der vollständigen Suiten.

## Abgleich mit A-NUM-01

| Anforderung | Ergebnis |
| --- | --- |
| 0,01 / 100 / 100.000 mm, Rotation und Roundtrip | vorhanden in `document.numeric_validation` |
| NaN/Inf, Nullachse, negative Maße | vorhandene Dokument-, Kernel- und Commandtests |
| Nicht normierte rechtshändige Ebenen | D10b1-Vertrag: Diagnose statt stiller Korrektur |
| Verbindung knapp unter/über Schwelle | D10c: Klassifikation und reale Faces, Kurvenlängen |
| Wiederholte Booleans ohne Sollmaßänderung | D10d: exakter Anschluss, Volumen und Bounds |
| ID-Überlauf vor Mutation | **offen, oben dynamisch widerlegt** |

Die frühere D10d-Matrix hatte die ID-Zeile nicht aufgeführt. Ihre Aussage über
eine vollständige A-NUM-01-Prüfung wird hiermit entsprechend eingeschränkt.
Grüne bestehende Suiten ersetzen diesen fehlenden Grenzfall nicht.

## Geprüfter Stand nach der Supportdatenkorrektur

- FreeBSD/Sway: **48/48**, 32,21 s – [Build](D10-review-artifacts/freebsd-sway/d10-review-build.log),
  [CTest](D10-review-artifacts/freebsd-sway/d10-review-ctest.log), [JUnit](D10-review-artifacts/freebsd-sway/ctest.xml).
- Windows: **52/52**, 95,08 s – [Build](D10-review-artifacts/windows/build.log),
  [CTest](D10-review-artifacts/windows/ctest.log), [JUnit](D10-review-artifacts/windows/ctest.xml).
- [250 Quellhashes](D10-review-artifacts/source-hashes.json) stimmen
  [plattformübergreifend überein](D10-review-artifacts/remote-source-verification.txt).
- Geänderte übertragene Quellen mit aktuellen Zeitstempeln neu kompiliert;
  keine Quellen während der Prüfläufe verändert. Source-Limits und diff-check
  bestanden, Originalfixtures unverändert. Alle gestarteten Prozesse beendet.

Die ältere D10a/D10b-Evidenz bleibt als historischer Teilnachweis erhalten.
Dieser Bericht dokumentiert den Reviewzeitpunkt; für den anschließenden Stand gilt D10e.
