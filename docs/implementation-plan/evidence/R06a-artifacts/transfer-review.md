# R06a – Transferreview der Kandidatenanalyse

Ziel: eduard@192.168.1.238:/home/eduard/src/voelund-p04-20260913/

Neun Quell-/Testdateien laut [Delta](source-delta.txt), [Prüfskript](verify-freebsd.sh),
Delta-/Quelllisten und [Hashmanifest](source-hashes.json).
Archiv: build/r06a-source-delta.tar.
SHA-256: d90436bf61820a51b40767e502a4a8a63a1f999c11c6ad49e4c87cea397258f5.

Read-only-Kandidatenanalyse und Application-Abfrage samt Regressionstest.
Isolierter Build mit OCCT 7.9.3, vollständige Produktsuite (erwartet 86 Tests),
zusätzlicher Referenzlauf. Logs und 363 Quellhashes zurückholen und vergleichen.
Keine UI-Änderung oder neue Aufnahme erforderlich. Keine Benutzerdateien,
Löschungen, Commits oder Pushes. Bestehende Testdateien liegen nur im Buildbaum.

Windows: Build, 89/89 Tests (109,50 s), vier Fixture-Ausgaben, Quellgrößen und
Diffprüfung bestanden. Payload übertragen und abgenommen: 86/86 FreeBSD-Tests (31,21 s), 363 identische Hashes und vier identische Fixture-Ausgaben.

Nach anfänglicher automatischer Ablehnung hat der Nutzer am 18.09.2026 eine dauerhafte
projektweite Freigabe für Dateitransfers, Builds und Tests auf dem dafür aufgesetzten
FreeBSD-Prüfrechner erteilt. Sie gilt auch für neue Pakete und spätere Sitzungen;
siehe [Ausführungsregel](../../10-autonome-umsetzung.md). R06a wurde daraufhin übertragen;
die Plattformprüfung ist erfolgreich abgeschlossen. [Gegenprüfung](final-verification.txt).
