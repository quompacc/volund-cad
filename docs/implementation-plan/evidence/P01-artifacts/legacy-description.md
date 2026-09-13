# P01 – Plattformnachweis und Testmatrix vorbereiten

**Gegenprüfung: Änderungen erforderlich, Status `review`.** Die nachstehende
ursprüngliche Abschlussbeschreibung ist durch die [Gegenprüfung](P01-review.md)
hinsichtlich der Abnahme überholt: Der Validator akzeptiert leere Pflichtdaten
und widersprüchliche Erfolgsbehauptungen. Vor P02 den dortigen Korrekturauftrag
abschließen; die historischen Windows-Laufergebnisse bleiben erhalten.

Stand: 13. September 2026. P01 bereitet den Plattformnachweis vor; es ist
**keine** Meridian-Abnahme und schließt weder G0 noch P04.

## Matrix

| Zielkonfiguration | Betriebssystem | Backend | QPA | Compositor | P01-Stand |
|---|---|---|---|---|---|
| Windows Produkt | Windows 11 | Qt-OpenGL | `windows` | nicht erfasst | bestanden |
| Windows Diagnose | Windows 11 | native WGL | `windows` | nicht erfasst | bestanden |
| FreeBSD Kontrolle | FreeBSD | Qt-OpenGL | `wayland` | Sway | blocked-external |
| FreeBSD Ziel | FreeBSD | Qt-OpenGL | `wayland` | Meridian | blocked-external |
| Linux Core | Linux | nicht anwendbar | nicht anwendbar | nicht anwendbar | blocked-external |

Die beiden Windows-Zeilen sind unterschiedliche tatsächlich ausgeführte
Backends (`qt-opengl` und `native`), nicht bloß unterschiedlich benannte
Qt-OpenGL-Presets. Linux Core verlangt keinen erfundenen GL-/Compositorwert.

## Aktuelle Evidenz

| Lauf | Buildpreset / Backend | Ergebnis | Bericht |
|---|---|---|---|
| `p01-windows-qt-opengl-20260913` | `windows-msvc` / Qt-OpenGL | 4/4 Desktop-CTests bestanden | [JSON](P01-artifacts/windows-qt-opengl/report.json), [JUnit](P01-artifacts/windows-qt-opengl/desktop-ctest.xml) |
| `p01-windows-native-20260913` | `windows-msvc-native-diagnostic` / native WGL | 4/4 Desktop-CTests bestanden | [JSON](P01-artifacts/windows-native/report.json), [JUnit](P01-artifacts/windows-native/desktop-ctest.xml) |

Die JSON-Berichte enthalten Run-ID, UTC-Zeit, Commit, Dirty-Markierung,
Quell-Dateihashes, Compiler, Qt/OCCT, tatsächliches Backend, QPA, GL/GPU,
Displaywerte, Paketstatus, Befehle, Exitcodes und Artefaktpfade. Nicht verfügbare
Werte sind mit Grund ausgefüllt, nicht geraten.

Der Agent hat aktuelle Aufnahmen betrachtet: [CAD Qt-OpenGL](P01-artifacts/windows-qt-opengl/captures/voelund-cad-widgets.png),
[Preview Qt-OpenGL](P01-artifacts/windows-qt-opengl/captures/voelund-ui-preview-widgets.png),
[CAD native framebuffer](P01-artifacts/windows-native/captures/voelund-cad-framebuffer.png)
und [Preview native framebuffer](P01-artifacts/windows-native/captures/voelund-ui-preview-framebuffer.png).
Sie zeigen jeweils Modellgeometrie; der CAD-Qt-Lauf außerdem Baum, Eigenschaften
und Transformationsgriff, der Preview-Lauf das aktive Extrude-Panel mit 31,00 mm.

Die Aufnahmegrenze bleibt ausdrücklich: `QWidget::grab()` und ein GL-Framebuffer
können getrennte Ebenen zeigen. Eine interne Qt-Aufnahme beweist keinen realen
Compositor; offscreen, XWayland oder historische Sway-/Meridian-Bilder wären kein
Ersatz für eine aktuelle native Meridian-/Wayland-Abnahme.

## Wiederholbarer Einstieg

```powershell
./tools/setup-windows.ps1 -Preset windows-msvc -BuildName p01-windows-qt-opengl-YYYYMMDD
./tools/setup-windows.ps1 -Preset windows-msvc-native-diagnostic -BuildName p01-windows-native-YYYYMMDD
# Je Build im Visual-Studio-Entwicklerkontext bauen, dann:
$env:VOELUND_SMOKE_ARTIFACT_DIR = "$PWD/docs/implementation-plan/evidence/P01-artifacts/windows-qt-opengl/captures"
./tools/New-PlatformReport.ps1 -Configuration windows-qt-opengl -BuildDirectory build/p01-windows-qt-opengl-YYYYMMDD -ArtifactDirectory "$PWD/docs/implementation-plan/evidence/P01-artifacts/windows-qt-opengl" -ReportPath "$PWD/docs/implementation-plan/evidence/P01-artifacts/windows-qt-opengl/report.json"
./tools/Test-PlatformReport.ps1 -ReportPath docs/implementation-plan/evidence/P01-artifacts/windows-qt-opengl/report.json
```

`New-PlatformReport.ps1` verwendet die bestehenden CTest-Smokes mit dem Label
`desktop`; CTest wird mit `--no-tests=error` aufgerufen, daher ist ein leerer
Pflichtfilter ein Fehler. Die Laufzeitdiagnostik aus `SmokeSupport` prüft die
Backendidentität; der Presetname allein genügt nicht.

`Test-PlatformReport.ps1` liefert Exitcode **0** für einen strukturell gültigen
Bericht, auch wenn die Matrix korrekt Blocker ausweist; Exitcode **1** bedeutet
einen Format-/Evidenzfehler. Mit `-RequirePlatformAcceptance` bedeutet Exitcode
**2**, dass der Bericht gültig, die gesamte fünfteilige Plattformabnahme aber
nicht bestanden ist. Damit bleiben „nicht ausgeführt“, „blockiert“ und
„fehlgeschlagen“ unterscheidbar.

## Validierungsregressionen

`pwsh -NoProfile -File tests/powershell/PlatformReport.Tests.ps1` bestand. Es
prüft einen vollständigen gültigen Bericht sowie fehlendes Pflichtfeld,
ungültigen Status, fehlende Begründung für `blocked`, fehlende Evidenz bei
`passed` und eine unvollständige Matrix. Ein vollständiger Bericht mit einem
korrekt ausgewiesenen Blocker bleibt strukturell gültig, wird aber mit
`-RequirePlatformAcceptance` nicht als Plattformabnahme akzeptiert.

## Externe Restnachweise

In diesem Arbeitsbereich sind keine dokumentierten und autorisierten
FreeBSD-Sway-, FreeBSD-Meridian- oder Linux-Hostzugänge vorhanden. Deshalb wurde
kein Remotehost gestartet oder Zugang erfunden. Für P04 ist ein echter
FreeBSD-Meridian-Host mit aktiver nativer Wayland-Sitzung erforderlich; dann
`sh tools/run-freebsd-integration.sh meridian <preset>` in dieser Sitzung
ausführen und die Interaktionen (Rendern, Pick, Orbit/Pan/Zoom, Resize,
Maximieren, Overlay) dort beobachten. Sway ist der Kontrolllauf, keine
Meridian-Ersetzung.

Verfügbare lokale Werkzeuge: CMake/Ninja/MSVC-Build, CTest, die Qt-basierten
Desktop-Smokes, SmokeSupport-Aufnahmen und lokale Bildbetrachtung. Die historischen
A08-Nachweise bleiben Kontext und wurden nicht als heutiger Meridianlauf ausgegeben.

## Paketabschluss

P01 ist als Vorbereitungs- und Berichtspaket abnahmefähig: Matrix, maschinenlesbare
Erzeugung, Validierung, Negativregressionen und aktuelle Windows-Nachweise liegen
vor; externe Plattformblocker sind präzise ausgewiesen. P04 und G0 bleiben offen.
