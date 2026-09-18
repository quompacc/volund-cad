# R04b – konkreter Transferumfang

Ziel: eduard@192.168.1.238:/home/eduard/src/voelund-p04-20260913/

28 Quell-/Testdateien gemäß [source-delta.txt](source-delta.txt), dazu
[Prüfskript](verify-freebsd.sh), Delta-/Quelllisten und die lokal generierte
Testdatei `windows-bound.volund` mit gebundener primitiver Fläche und Extrusionskappe.

Archiv: `build/r04b-source-delta.tar`

SHA-256: `7eb931794b685eec94aac1cc8950593d40b7bf64c0a872b655c05338be532558`

Das Skript aktualisiert die genannten Dateien im bestehenden Prüfbaum, baut mit
dem isolierten OCCT-7.9.3-Prefix, führt die Produktsuite unter FreeBSD/Sway aus,
liest/schreibt die Bindungsfixture und erzeugt Hashes für 339 Quell-/Fixturedateien.
Die beiden FreeBSD-Dateien werden für die Windows-Rückprüfung zurückgeholt.
Kein Git-Push, keine Löschungen. Enthalten ist interner Projektquelltext und eine
synthetische Testdatei; keine Zugangsdaten. Status: nach Nutzerantwort „weiter“ übertragen und vollständig geprüft. 80/80 FreeBSD-Tests bestanden; 339 Hashes identisch. Beide FreeBSD-Dateien unter Windows erfolgreich geprüft.

Windows-Voraussetzungen: 83/83 Tests bestanden (101,20 s), Bindungsfixture lokal
geschrieben. Automatische Freigabeprüfung hat den Transfer am 17.09.2026 wegen
fehlender ausdrücklicher Freigabe für diesen R04b-Payload abgelehnt. Die konkrete
Nutzerfreigabe wurde anschließend mit „weiter“ erteilt; die erneute automatische Prüfung hat den unveränderten Transfer zugelassen.
