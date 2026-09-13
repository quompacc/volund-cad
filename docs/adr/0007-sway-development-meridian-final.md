# ADR 0007: Auf FreeBSD/Sway entwickeln, Meridian abschließend integrieren

**Status:** accepted – ausdrückliche Nutzerentscheidung
**Datum:** 13. September 2026
**Owner:** Völund-Projekt

## Kontext und Entscheidung

Völund wird zunächst auf dem vorhandenen FreeBSD-Rechner unter Sway entwickelt
und getestet. Meridian wird erst nach Stabilisierung des CAD-Tools integriert.
So lassen sich CAD-, Qt-/OCCT- und Compositorfehler getrennt untersuchen.

Die Alternative, Meridian bereits vor weiterer CAD-Entwicklung vorauszusetzen,
wird auf ausdrücklichen Nutzerwunsch verworfen. Diese Entscheidung ersetzt die
frühe Meridian-Sperre und die Entwicklungsreihenfolge aus ADR 0004.

## Konsequenzen

- P04 weist jetzt die native FreeBSD/Sway-Entwicklungsbasis nach: aktueller Build,
  vollständiger CTest, produktive GUI-Smokes, Lifecycle/Picking/Navigation/
  Manipulator, Größenwechsel und betrachtete Qt- sowie Compositoraufnahmen.
- Abhängige CAD-Pakete dürfen nach dieser P04-Abnahme weiterlaufen. Ihre eigenen
  Abhängigkeiten und Qualitätsregeln bleiben verbindlich; G0 ist dadurch nicht
  automatisch vollständig abgeschlossen.
- Windows bleibt die zusätzliche Build-/Regressionsebene. FreeBSD/Sway wird bei
  relevanten Änderungen fortlaufend geprüft, nicht erst wieder zum Release.
- Die echte Meridian-Integration einschließlich Portalen, Theme, Fokus,
  Clipboard, Suspend/Resume und installierter Laufzeit bleibt L07 am Ende der
  Releasephase. Sway-Nachweise werden niemals als Meridian-Nachweise bezeichnet.
- Völund bleibt ein unabhängiger Qt-Wayland-Client ohne private Meridian-
  Abhängigkeiten. Der native Wayland-/EGL-Weg aus ADR 0004/0006 bleibt erhalten.
- Keine Meridian-Installation oder Änderung des laufenden Sway-Desktops im
  aktuellen CAD-Entwicklungsschritt.

Keine befristete technische Ausnahme: Eine erneute Änderung dieser Reihenfolge
benötigt eine neue Produktentscheidung. Historische Evidenz bleibt datiert erhalten.
