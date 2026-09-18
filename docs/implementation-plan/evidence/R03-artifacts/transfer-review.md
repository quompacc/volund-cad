# R03 – konkreter Transferumfang

Ziel: eduard@192.168.1.238:/home/eduard/src/voelund-p04-20260913/

8 Quell-/Testdateien plus verify-freebsd.sh (remote verify-r03-freebsd.sh), Delta- und Quellprüfliste.
Archiv: build/r03-source-delta.tar
SHA-256: 7c41d00cc17c1ee76c7c83b5d9925b60a6f4caec0abccd21954131e6fd88562b

Das Skript entpackt im bestehenden Prüfbaum, baut mit dem isolierten OCCT-7.9.3-Prefix,
führt die Produktsuite unter FreeBSD/Sway aus und erzeugt Hashes für 328 Quell-/Fixturedateien.
Kein Git-Push und keine Löschungen. Übertragen wird interner Projektquelltext auf den angegebenen Host.

Nach anfänglicher automatischer Ablehnung hat der Nutzer genau diesen Transfer
und die FreeBSD-Prüfung ausdrücklich freigegeben. Transfer und Prüfung ausgeführt:
78/78 Tests bestanden, 328/328 Quell-/Fixture-Hashes identisch zum lokalen Stand.

Dateien:

- include/voelund/kernel/OperationHistory.hpp
- include/voelund/kernel/ShapeFactory.hpp
- include/voelund/kernel/ShapeOperations.hpp
- src/kernel/OperationHistoryDetail.hpp
- src/kernel/ShapeFactory.cpp
- src/kernel/ShapeOperations.cpp
- tests/CMakeLists.txt
- tests/integration/OperationHistoryTests.cpp
