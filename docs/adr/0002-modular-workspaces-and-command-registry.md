# ADR 0002: Modulare Arbeitsbereiche und Command-Registry

**Status:** angenommen
**Datum:** 10. September 2026

## Kontext

Völund soll zunächst den Arbeitsbereich `Solid` vollständig entwickeln. Spätere
Arbeitsbereiche wie Surface, Mesh, Sheet Metal und Assembly dürfen keine Umbauten
am CAD-Kern oder fest verdrahtete Sonderleisten in `MainWindow` erfordern.

## Entscheidung

- Ein Arbeitsbereich implementiert die öffentliche Schnittstelle `WorkspaceModule`.
- Ein Modul registriert ausschließlich tatsächlich verfügbare Commands.
- Jeder Command besitzt eine global eindeutige ID, eine Werkzeuggruppe, Angaben zur
  Schnellwahl, Aktivierungslogik und seine Ausführung.
- `CommandRegistry` besitzt die registrierten Module und lehnt doppelte Modul- oder
  Command-IDs ab.
- Die Desktop-Oberfläche erzeugt Schnellwahl und Gruppenmenüs aus den Deskriptoren.
- Der CAD-Kern kennt keine konkreten Arbeitsbereichsmodule.
- Module werden zunächst statisch als C++-Bibliotheken gelinkt. Eine dynamische
  Plug-in-ABI wird erst eingeführt, wenn ein realer Auslieferungsfall sie erfordert.

## Konsequenzen

`Solid` ist das erste Produktmodul. Weitere Arbeitsbereiche können dieselbe
Registry und dieselbe Desktop-Darstellung verwenden. Neue Module benötigen eine
explizite Registrierung im Composition Root, aber keine Änderung an Registry,
Command-Vertrag oder Feature-Kern.
