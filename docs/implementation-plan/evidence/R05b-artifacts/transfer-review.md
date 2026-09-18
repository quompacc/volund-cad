# R05b – Transfer des Kontur-Prüfschritts

Ziel: eduard@192.168.1.238:/home/eduard/src/voelund-p04-20260913/

11 Quell-/Testdateien laut [Delta](source-delta.txt), [Prüfskript](verify-freebsd.sh)
und Delta-/Quelllisten. Archiv: `build/r05b-contour-source-delta.tar`.
SHA-256: `89dd6e76585666809e040797616c40fe8cf235a017c5a19b8d68f099646c8a43`.

Das Skript aktualisiert genau diese Dateien im bestehenden Prüfbaum, baut mit
isoliertem OCCT-7.9.3-Prefix, führt die vollständige Produktsuite unter Sway/Wayland
und den Konturtest aus und erzeugt 347 Quell-/Fixture-Hashes. Logs und Hashes werden
zur lokalen Abnahme zurückgeholt. Keine Löschungen, kein Commit/Push, keine Zugangsdaten
im Payload. Native Schema 2 bleibt unverändert; persistente Kantenconstraints und
UI-Anbindung sind in diesem Payload noch nicht enthalten.

Windows: vollständiger Build, 86/86 Tests (109,07 s), Kontur- und Primitive-Kantenlauf
sowie Quellgrößenprüfung bestanden. Status: übertragen und plattformübergreifend geprüft.

Automatische Freigabeprüfung hat den Transfer vor Ausführung abgewiesen: bisherige
Freigabe umfasste nur R05a. Danach ausdrückliche Nutzerfreigabe für diesen Payload,
erneute automatische Prüfung erfolgreich. Alle elf Dateien samt Prüfskript und Listen
übertragen, Build und 83/83 FreeBSD-Tests (30,28 s) bestanden. Prüflogs zurückgeholt;
347 Quell-/Fixture-Hashes und drei Konturfixture-Ausgaben stimmen überein.
