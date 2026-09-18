# R04c – konkreter Transferumfang

Ziel: eduard@192.168.1.238:/home/eduard/src/voelund-p04-20260913/

17 Quell-/Testdateien gemäß [source-delta.txt](source-delta.txt), dazu
[Prüfskript](verify-freebsd.sh) und Delta-/Quelllisten.

Archiv: `build/r04c-source-delta.tar`

SHA-256: `fc1c9dbd48f8db27f29d92890be8f37e45e23a5d88852c24c1e27ab82327a616`

Das Skript aktualisiert die genannten Dateien im bestehenden Prüfbaum, baut mit
dem isolierten OCCT-7.9.3-Prefix, führt die Produktsuite unter FreeBSD/Sway aus,
erzeugt UI-Aufnahmen und prüft 341 Quell-/Fixture-Hashes. Logs, Hashes und die
UI-Aufnahmen werden zur lokalen Abnahme zurückgeholt. Die Tests erzeugen ihre
synthetischen Dokumente selbst. Kein Git-Push, keine Löschungen; enthalten sind
interner Projektquelltext und Testcode, keine Zugangsdaten.

Status: nach ausdrücklicher Nutzerfreigabe „Ja, R04c-Transfer und FreeBSD-Abnahme
freigeben“ übertragen; Prüfung abgeschlossen. Die anfängliche automatische Ablehnung
wegen fehlender Payload-Freigabe wurde damit aufgelöst. Windows: 84/84 Tests,
104,82 s; UI-Aufnahmen selbst geprüft.

FreeBSD: 81/81 Tests, 29,85 s; 341 Quell-/Fixture-Hashes identisch. Beide UI-
Aufnahmen zurückgeholt und selbst geprüft. Kein Commit/Push.
