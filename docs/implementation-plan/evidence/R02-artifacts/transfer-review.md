# R02 – konkreter FreeBSD-Prüfumfang

Ziel: `eduard@192.168.1.238:/home/eduard/src/voelund-p04-20260913/`.

Archiv: `build/r02-reference-contract.tar`.
SHA-256: `e4cff3424e8fbceedd7c8e42fb8c046d344578c0fea8283d5d67f52e2c417ff8`.

Enthält ausschließlich diese drei neuen Dateien:

- `tools/reference-contract/CMakeLists.txt`
- `tools/reference-contract/SubshapeContract.hpp`
- `tools/reference-contract/ContractProof.cpp`

Zusätzlich wird das hier abgelegte `verify-freebsd.sh` als
`verify-r02-freebsd.sh` übertragen und ausgeführt. Es entpackt die drei Dateien,
baut den isolierten C++20-Vertragsnachweis unter `build/r02-reference-contract`,
führt die 26 symbolischen Fixtures und CTest aus und schreibt Logs/Quellhashes.
Die bestehenden unveränderten DocumentId-/BodyId-/FeatureId-/IdCapacity-Header
werden aus dem bereits geprüften Projektbaum gelesen.

Kein Produktumbau, keine Löschung, kein Git-Push. Übertragen wird interner
Projektquelltext auf den genannten Host. Die automatische Freigabeprüfung hat
diesen neuen Payload abgelehnt, weil die R01-Freigabe ihn nicht ausdrücklich
umfasst. Anschließend hat der Nutzer genau diesen Transfer und die Prüfung
ausdrücklich freigegeben. Upload und FreeBSD-Lauf wurden erfolgreich ausgeführt;
26/26 Fixtures, 1/1 CTest und 3/3 identische Quellhashes sind belegt.
