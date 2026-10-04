# ADR 0011: Linux-Desktop als gleichrangiges Ziel, CI auf dem Testrechner

**Status:** accepted – ausdrückliche Nutzerentscheidung
**Datum:** 4. Oktober 2026
**Owner:** Völund-Projekt
**Ergänzt:** [ADR 0004](0004-freebsd-first-meridian-integration.md), [ADR 0007](0007-sway-development-meridian-final.md)

## Kontext

Völund wurde bisher FreeBSD-first ausgelegt; Linux war nur als Core-Build ohne
Desktop vorgesehen (`linux-gcc`, `VOELUND_BUILD_DESKTOP=OFF`). Der Nutzer plant
den Einsatz auch unter Linux. Der bisherige FreeBSD-Testrechner läuft inzwischen
nativ mit Fedora.

Jedes Paket wurde bisher manuell auf Windows und FreeBSD gebaut, mit Quellhash-Listen
abgeglichen und mit eingecheckten Logs und Aufnahmen belegt. Eine CI war erst für G4
(L01) geplant.

## Entscheidung

- Linux-Desktop mit nativem Wayland ist ein gleichrangiges Produktziel neben
  FreeBSD/Meridian und Windows. Referenzumgebung ist Fedora auf dem Testrechner.
- Der Testrechner dient als Gitea-Actions-Runner. Eine Pflicht-CI (C01) läuft je
  Push und ersetzt die manuelle Hash- und Logevidenz.
- Windows wird je Paket lokal geprüft, bis ein Windows-Runner existiert.
- FreeBSD/Sway bleibt unterstützt und wird je Gate-Abnahme geprüft; Meridian bleibt
  L07. Sway- oder Fedora-Nachweise gelten nie als Meridian-Nachweis.

## Konsequenzen

- Neue Pakete P05 (Linux-Desktopbasis) und C01 (Pflicht-CI) stehen vor R09.
  L01 baut nur noch die Release-Matrix auf C01 auf; L08 nimmt Windows und
  Linux-Desktop ab.
- Der native Wayland-/EGL-Weg aus ADR 0004/0006 gilt für Linux unverändert:
  Qt besitzt Surface und Kontext, kein X11/XWayland als Abnahmeersatz.
- Keine Linux-spezifischen Produktabhängigkeiten außerhalb des bestehenden
  Dependency-Budgets; Paketform (z. B. RPM oder Flatpak) wird in L02 entschieden.
- Bestehende Evidenz bleibt datiert gültig. Neue Paketnachweise verweisen auf
  CI-Läufe; Logs und Aufnahmen werden nicht mehr eingecheckt.
