# R04c – eigene Sichtprüfung

## Windows

Die zwei vom produktiven MainWindow erzeugten PNGs wurden am 17.09.2026 geöffnet
und selbst geprüft:

- `windows-ui/bound-rotated.png`: Editor ist zur gedrehten Fläche ausgerichtet;
  grüne geschlossene Kontur und Punktmarkierungen liegen auf der Stützebene.
  Achsen, Editorwerkzeuge sowie OK/Cancel sind sichtbar. Kein Fehlerzustand.
- `windows-ui/bound-deleted.png`: Modellbaum zeigt `Sketch 1 [error]` und
  `Extrusion 1 [blocked]`. Das Fehlerpanel nennt `Sketch support face was deleted`.
  Die unabhängige Box bleibt ohne Fehlerkennzeichnung. Sichtbare alte Geometrie
  ist unten ausdrücklich mit `Last valid geometry` gekennzeichnet; keine alte
  Skizzenkontur wird als aktuelles Ergebnis darübergelegt.

Der Lauf meldet eine fehlende OCCT-Schrift `Courier`; diese bekannte Umgebungsmeldung
verhindert die geprüften Qt-Beschriftungen und die Abnahme der dargestellten Fälle
nicht. Die Aufnahmen sind kontrollierte Testzustände, kein manuell ausgeführter
Maus-Picking-Nachweis.

## FreeBSD

Beide zurückgeholten PNGs am 17.09.2026 selbst geöffnet und geprüft:

- `r04c-ui/bound-rotated.png`: grüne geschlossene Kontur auf der gedrehten Ebene,
  Editorachsen und OK/Cancel sichtbar; Ansicht korrekt zur Fläche ausgerichtet.
- `r04c-ui/bound-deleted.png`: Fehler an der Skizze, blockierte Extrusion,
  unabhängige Box ohne Fehlerstatus; konkreter Löschungsgrund im roten Panel und
  eindeutige Kennzeichnung letzter gültiger Geometrie in der Statuszeile.

Qt-Stil, Hintergrundfarbe und Punktmarkierungsdarstellung unterscheiden sich von
Windows. Geprüfte Lage, Kontur und Fehleraussage stimmen überein.
