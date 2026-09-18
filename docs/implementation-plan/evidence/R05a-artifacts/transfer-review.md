# R05a – konkreter Transferumfang

Ziel: eduard@192.168.1.238:/home/eduard/src/voelund-p04-20260913/

7 Quell-/Testdateien gemäß [source-delta.txt](source-delta.txt), dazu
[Prüfskript](verify-freebsd.sh) und Delta-/Quelllisten.

Archiv: `build/r05a-source-delta.tar`

SHA-256: `f8a7aa04e4f38a2174ff27c9c8cb47e912f033e8aa6e7c6926b871b5d575429c`

Das Skript aktualisiert die genannten Dateien im bestehenden Prüfbaum, baut mit
dem isolierten OCCT-7.9.3-Prefix und führt die Produktsuite unter FreeBSD/Sway aus.
Kantenfixtures werden separat protokolliert und 344 Quell-/Fixture-Hashes erzeugt.
Logs und Hashes werden zur lokalen Abnahme zurückgeholt. Kein Git-Push und keine
Löschungen. Interner Projektquelltext und Testcode, keine Zugangsdaten.

Status: nach Nutzerfreigabe übertragen und plattformübergreifend geprüft.

Windows: 85/85 Tests bestanden (104,36 s), zusätzlicher Kantenlauf bestanden.
Die automatische Freigabeprüfung hatte die erste Übertragung vor Ausführung
abgewiesen. Der Nutzer hat die konkrete Bitte anschließend mit „weiter“ freigegeben;
die erneute automatische Prüfung ließ den Transfer zu.
FreeBSD: vollständiger Build, 82/82 Tests (29,90 s), separater Kantenlauf bestanden.
344 Quell-/Fixture-Hashes und alle vier Kantenfixture-Ausgaben stimmen überein.
Prüflogs und Hashlisten zurückgeholt; alle Prüfprozesse beendet.
