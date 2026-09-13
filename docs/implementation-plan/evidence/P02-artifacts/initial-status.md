# P02 – Viewport-Gemeinsamkeiten und GL-Lebensdauer

**Status: in progress. Nicht abnahmefähig.**

## Verantwortlichkeit und Besitz

| Bereich | Eigentümer | Kontextgebunden |
|---|---|---|
| Bodies, Sketches, Auswahl-IDs, Kameraabsicht und Werkzeugzustand | `CadViewport::Impl` als CPU-/Szenenzustand | nein |
| AIS-Präsentationen, Viewer, View, Manipulator-Anbindung und Qt-FBO-Wrapper | Viewport-Backend | ja |
| Widget, WGL-/Qt-Kontextaktivierung und Framebufferbindung | jeweiliges Backend | ja |

Die bisher doppelt vorhandenen öffentlichen Szenen-, Kamera-, Auswahl- und
Eingabemethoden werden nun aus `CadViewport.cpp` und
`CadViewportNativeInput.cpp` für beide Backends gebaut. Backend-spezifisch
bleiben nur die Qt-GL-Callbacks bzw. die native Fenster-/WGL-Anbindung.

## Lebensdauervertrag

`uninitialisiert -> initialisierend -> bereit -> Cleanup/Kontextverlust -> erneut initialisierbar`.

Beim Qt-Weg wird der tatsächlich gebundene `QOpenGLContext` beobachtet.
`aboutToBeDestroyed` markiert die Ansicht sofort als nicht bereit, räumt bei
gültigem eigenen aktuellem Kontext auf und verwirft alte OCCT-Präsentationen.
Der nächste `initializeGL()`-Aufruf erzeugt die OCCT-Szene neu und rekonstruiert
sie aus dem erhaltenen CPU-Zustand. Die schreibgeschützten Generationen dienen
der Diagnose; ein global fremder aktueller Kontext wird nicht verwendet.

## Noch offene Abnahme

- Vollständige Builds und CTests beider Windows-Backends stehen nach der
  Umstellung noch aus.
- Der neue Qt-Lifecycle-CTest wird gebaut, muss aber noch vollständig laufen;
  Reparenting wird dabei nur als echter Context-Recreate gezählt, wenn die
  Kontextgeneration steigt.
- Aktuelle Aufnahmen beider Backends wurden noch nicht erzeugt oder betrachtet.
- P02 ist deshalb nicht als `done` markiert; P03 wurde nicht begonnen.
