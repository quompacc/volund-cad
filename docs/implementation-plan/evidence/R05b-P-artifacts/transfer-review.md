# R05b-P – Transfer für Referenzpersistenz und Dateiaustausch

Ziel: eduard@192.168.1.238:/home/eduard/src/voelund-p04-20260913/

22 Quell-/Testdateien laut [Delta](source-delta.txt), [Prüfskript](verify-freebsd.sh),
Delta-/Quelllisten und die native Windows-Testdatei `windows-projected-edge.volund`.
Archiv: `build/r05bp-source-delta.tar`.
SHA-256: `451f33b238d5339aee6c3f1aeb21a3136cb032065e5cef30229372b775eec2e3`.

Der bestehende Prüfbaum wird für Anwendungsschema 3 aktualisiert; Container 12 bleibt.
Isolierter OCCT-7.9.3-Build, vollständige Produktsuite, zusätzliche Referenzfixtures,
Einlesen der Windows-Datei und Schreiben einer eigenen FreeBSD-Testdatei.
Logs, FreeBSD-Datei und 353 Quellhashes werden zur lokalen Gegenprüfung zurückgeholt.
Keine Benutzerdateien überschreiben, keine Löschungen, kein Commit/Push und keine
Zugangsdaten im Payload. UI-Anbindung ist noch nicht enthalten.

Status: nach Nutzerfreigabe „weiter“ übertragen und abgenommen. Windows-Build und 87/87 Tests (116,42 s), gezielter Referenzlauf, Quellgrößen und Diffprüfung bestanden.

Die anfängliche automatische Ablehnung wurde durch die konkrete Nutzerfreigabe aufgehoben. FreeBSD-Build und 84/84 Tests (30,96 s) bestanden; 353 Hashes und drei Fixture-Ausgaben identisch. Nativer Dateiaustausch in beiden Richtungen bestanden. [Gegenprüfung](exchange-verification.txt).
