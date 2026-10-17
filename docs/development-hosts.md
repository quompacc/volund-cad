# Testrechner

## Build-Orte (Nutzerentscheidung 14.10.2026)

Es gibt genau diese Orte; neue Build-Ordner werden nicht angelegt:

| Zweck | Ort |
|---|---|
| Startbares Programm (Windows) | `Installer\` im Projekt, `Installer\bin\voelund-cad.exe`, mit allen DLLs und Qt-Plugins |
| Zwischendateien des Compilers (Windows) | `build\p02-qt-final` (Debug, MSVC, vcpkg); nur über `subst V:` bauen |
| GCC-Prüfung und Desktop-Tests | Fedora `~/repro/par01/build/linux-desktop` |
| Abnahme | CI auf dem Gitea-Runner |

Beide lokalen Ordner stehen in `.gitignore`. Ablauf unter Windows:

```bat
subst V: "D:\300_Projekte\310_Aktiv\volund-cad"
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
V:
cd \
cmake --build build/p02-qt-final
cmake --install build/p02-qt-final --prefix Installer
Installer\bin\voelund-cad.exe
```

Tests legen ihre Hilfsdateien nur für die eigene Laufzeit an und räumen sie wieder weg
(am 14.10.2026 lagen 235 vergessene Ordner eines PowerShell-Tests und rund 70 alte
Paket-Builds mit zusammen etwa 24 GB in `build\`; gelöscht).

## Aktuell: Fedora (seit Oktober 2026)

Der Testrechner läuft laut Nutzer seit Anfang Oktober 2026 nativ mit Fedora und
soll als Gitea-Actions-Runner dienen ([ADR 0011](adr/0011-linux-desktop-and-ci-runner.md)).
Am 04.10.2026 in P05 per SSH verifiziert:

| Eigenschaft | Verifizierter Stand |
|---|---|
| SSH | `eduard@192.168.1.203`, Schlüssel `%USERPROFILE%/.ssh/meridian_freebsd` |
| Hostname / Betriebssystem | `fedora-dev`, Fedora Linux 44 (KDE Plasma), Kernel 7.2.8 |
| Hardware | 4 Kerne, 15 GB RAM; Intel HD 620 (aktiv) und NVIDIA GM107 (nicht geprüft) |
| Grafiksitzung | KDE Plasma, `kwin_wayland`, `WAYLAND_DISPLAY=wayland-0`, `XDG_RUNTIME_DIR=/run/user/1000` |
| Ausgabe | eDP-1, 1920 × 1080, Faktor 1 |
| Mesa / EGL | Mesa 26.2.3, EGL 1.5, OpenGL ES 3.2 |
| Qt / Compiler / CMake | Qt 6.11.2 (Fedora), GCC 16.2.1, CMake 4.3.0, Eigen 5.0.1, Boost 1.90 |
| OCCT | projektlokal 7.9.3 unter `.tools/occt-linux/install` |
| Prüfverzeichnis | `/home/eduard/src/volund-cad` (kein Git-Checkout) |
| Aufnahme | `spectacle -b -n -f -o <datei>` |
| sudo | mit Passwort; Paketinstallation erfolgt durch den Nutzer |

Verbindung aus PowerShell:

```powershell
ssh -i "$env:USERPROFILE/.ssh/meridian_freebsd" -o IdentitiesOnly=yes eduard@192.168.1.203
```

Native Wayland-Läufe über SSH setzen `XDG_RUNTIME_DIR=/run/user/1000` und
`WAYLAND_DISPLAY=wayland-0`; `DISPLAY` bleibt ungesetzt.

Desktop-Prüfweg (P05): `tools/setup-linux.sh` (Fedora-Pakete prüfen, projektlokales
OCCT 7.9.3 bauen, Preset `linux-desktop` konfigurieren), danach in der aktiven
Wayland-Sitzung `sh tools/run-linux-desktop.sh`. Es schreibt Hostinventar, EGL-Info,
Bibliotheksliste und CTest/JUnit nach `artifacts/<preset>/`; ohne Argument ist das Preset
`linux-desktop`, für Release `sh tools/run-linux-desktop.sh linux-desktop-release`.

Der SSH-Alias `volund` (`192.168.1.19`, `volund-lxc`) ist ein Debian-Container eines
anderen Projekts und **nicht** der Testrechner.

### Gitea-Runner (C01)

- Dienst `gitea-runner.service` (System-Unit, `User=eduard`), Binärdatei
  `/usr/local/bin/gitea-runner` v4.1.0, Konfiguration `~/gitea-runner/config.yaml`
  aus `tools/ci/gitea-runner.yaml`, Registrierung `~/gitea-runner/.runner` (nicht im Repo).
- Label `linux-desktop:host`, Kapazität 1, Instanz `https://gitea.hl.home.arpa`;
  der Host vertraut dafür der Homelab-CA „HL Home Root CA“ (SHA-1 `2DAF…9B90`).
- Einrichtung einmalig: Token nach `~/gitea-runner/token`, dann
  `sudo sh ~/gitea-runner/install-runner.sh` (Kopie von `tools/ci/install-runner.sh`).
- Jobs laufen in `~/gitea-runner/work`; OCCT kommt aus `~/.cache/voelund/occt-linux`.
- Desktoptests standardmäßig in headless `kwin_wayland --virtual` auf der Intel-GPU
  (`renderD128`), unabhängig von einer Anmeldung; `VOELUND_CI_SESSION=desktop`
  verwendet stattdessen die angemeldete Sitzung.
- CI-Stand prüfen: Lese-Token des Nutzers (nur lesend, für Agenten angelegt am 04.10.2026)
  in `~/.config/volund/gitea-read-token` (Modus 600). Der Token gehört nicht ins Repo.
  Lauf-ID (`id`) ist die in Nachweisen zitierte Nummer, `run_number` das „ci #N“ der Oberfläche.

```sh
curl -fsS -H "Authorization: token $(cat ~/.config/volund/gitea-read-token)" \
  "https://gitea.hl.home.arpa/api/v1/repos/eduard/volund-cad/actions/runs?limit=5"
```

## Historisch: FreeBSD-Testrechner (September 2026)

Die folgenden Angaben beschreiben den Stand bis zur Neuinstallation und belegen
die bisherigen FreeBSD/Sway-Nachweise.

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
Zustimmung je Transfer einholen. Verbindlich: [Ausführungsregel](implementation-plan/10-autonome-umsetzung.md#dauerhafte-freigabe-für-den-testrechner). Der vorhandene Projektstand
wird nicht durch den aktuellen Arbeitsbaum überschrieben. Eine laufende oder
installierte Meridian-Sitzung wurde bislang nicht gefunden; Sway bleibt der
Kontrollcompositor und wird nicht als Meridian ausgegeben.

## Verbindliche Entwicklungsreihenfolge

Nutzerentscheidung vom 13.09.2026: auf diesem FreeBSD/Sway-System weiterentwickeln.
Meridian erst abschließend in L07 integrieren; siehe [ADR 0007](adr/0007-sway-development-meridian-final.md).
P04-Sway-Basis: 23/23 CTests, Lifecycle und betrachtete Compositoraufnahmen.
