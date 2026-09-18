# Eingerichteter FreeBSD-Testrechner

Am 13.09.2026 vom Nutzer ausdrücklich für diese Tests benannt und per SSH geprüft.
Vor Aussagen über fehlende Plattformzugänge diese Datei und die lokale SSH-
Konfiguration prüfen. Keine privaten Schlüssel oder Passwörter hier ablegen.

| Eigenschaft | Verifizierter Stand |
|---|---|
| SSH | `eduard@192.168.1.238` |
| Hostname / Betriebssystem | `freebsd-dev`, FreeBSD 15.1-RELEASE-p3, amd64 |
| Vorhandener SSH-Schlüssel unter Windows | `%USERPROFILE%/.ssh/meridian_freebsd` |
| Bestehender Projektstand | `/home/eduard/src/voelund`, kein Git-Checkout |
| Vorhandene schlanke OCCT-Installation | `/home/eduard/src/voelund/.tools/occt-freebsd/install` |
| Qt / OCCT | Qt 6.11.1 mit nativem Wayland-Plugin / OCCT 7.9.3 |
| Grafiksitzung | Sway 1.12, Benutzer eduard |
| Runtime / Wayland-Socket | `/var/run/xdg/eduard`, `wayland-1` |
| Ausgabe | eDP-1, 1920 × 1080, Faktor 1, Chimei Innolux 0x15C4 |
| Aufnahme | `grim` vorhanden |
| P04-Prüfverzeichnis | `/home/eduard/src/voelund-p04-20260913` |

Verbindung aus PowerShell:

```powershell
ssh -i "$env:USERPROFILE/.ssh/meridian_freebsd" -o IdentitiesOnly=yes eduard@192.168.1.238
```

Der vorhandene Alias `meridian-openbsd` zeigt zwar auf dieselbe IP, benutzt aber
einen anderen Schlüssel. Nicht aus diesem Alias auf das aktuelle Betriebssystem
schließen. Der Standard-SSH-Aufruf ohne expliziten Schlüssel wurde abgewiesen;
der obige Zugang funktioniert.

Sway-IPC-Socket jeweils neu unter `/var/run/xdg/eduard` ermitteln, da seine PID
wechselt. Native Wayland-Läufe verwenden dieses Runtime-Verzeichnis und
`WAYLAND_DISPLAY=wayland-1`, `QT_QPA_PLATFORM=wayland`, ohne DISPLAY.
Sitzung/Socket vor Verwendung erneut prüfen.

Der Nutzer hat am 18.09.2026 die **dauerhafte projektweite Freigabe** erteilt:
Projektdateien einschließlich neuer interner Quelltexte, Tests und Prüfdaten dürfen
auf diesen ausdrücklich dafür aufgesetzten Host übertragen werden. Builds und Tests
sowie Rücktransfer und Auswertung ihrer Artefakte sind eingeschlossen. Die Freigabe
gilt auch für neue Pakete, korrigierte Payloads und spätere Sitzungen; keine erneute
Zustimmung je Transfer einholen. Verbindlich: [Ausführungsregel](implementation-plan/10-autonome-umsetzung.md#dauerhafte-freigabe-für-den-freebsd-prüfrechner). Der vorhandene Projektstand
wird nicht durch den aktuellen Arbeitsbaum überschrieben. Eine laufende oder
installierte Meridian-Sitzung wurde bislang nicht gefunden; Sway bleibt der
Kontrollcompositor und wird nicht als Meridian ausgegeben.

## Verbindliche Entwicklungsreihenfolge

Nutzerentscheidung vom 13.09.2026: auf diesem FreeBSD/Sway-System weiterentwickeln.
Meridian erst abschließend in L07 integrieren; siehe [ADR 0007](adr/0007-sway-development-meridian-final.md).
P04-Sway-Basis: 23/23 CTests, Lifecycle und betrachtete Compositoraufnahmen.
