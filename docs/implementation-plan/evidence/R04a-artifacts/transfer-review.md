# R04a – konkreter Transferumfang

Ziel: eduard@192.168.1.238:/home/eduard/src/voelund-p04-20260913/

12 Quell-/Testdateien plus verify-freebsd.sh (remote verify-r04a-freebsd.sh), Delta- und Quellprüfliste.
Archiv: build/r04a-source-delta.tar
SHA-256: 8c4bbfc837ae4e15268be5fda07165abbf8d8c5a87240fa1f97adc6bc25dc016

Das Skript entpackt im bestehenden Prüfbaum, baut mit dem isolierten OCCT-7.9.3-Prefix,
führt die Produktsuite unter FreeBSD/Sway aus und erzeugt Hashes für 332 Quell-/Fixturedateien.
Kein Git-Push und keine Löschungen. Übertragen wird interner Projektquelltext auf den angegebenen Host.

Transfer und Prüfung nach Nutzerantwort „weiter“ auf die konkrete Freigabebitte
ausgeführt; die erneute automatische Prüfung hat den Transfer zugelassen.
79/79 FreeBSD-Tests bestanden, 332 Quell-/Fixture-Hashes identisch zum lokalen Stand.

Dateien:

- include/voelund/features/BoxFeature.hpp
- include/voelund/features/CylinderFeature.hpp
- include/voelund/features/FeatureGraph.hpp
- include/voelund/features/SupportProvenance.hpp
- src/features/BoxFeature.cpp
- src/features/CMakeLists.txt
- src/features/CylinderFeature.cpp
- src/features/FeatureGraph.cpp
- src/features/PrimitivePlacement.hpp
- src/features/SupportProvenance.cpp
- tests/CMakeLists.txt
- tests/integration/SupportProvenanceTests.cpp
