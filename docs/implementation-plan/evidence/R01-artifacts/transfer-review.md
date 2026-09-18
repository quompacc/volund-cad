# R01 – konkreter Transferumfang

Ziel: eduard@192.168.1.238:/home/eduard/src/voelund-p04-20260913/

16 Quell-/Testdateien plus verify-freebsd.sh, Quellliste und Hash-Prüfliste. Das Skript entpackt im bestehenden isolierten Prüfbaum, baut mit dem isolierten OCCT-Prefix und führt CTest unter FreeBSD/Sway aus. Keine Löschungen, kein Git-Push.

Archiv: build/r01-source-delta.tar
SHA-256: ef5a4f39dac43e12900ac403a32568ef50e3936ca81cbec4c7ca83d325e19e3b

Enthaltene Dateien:

- include/voelund/application/CadSession.hpp
- include/voelund/features/EvaluationState.hpp
- include/voelund/features/FeatureDependencies.hpp
- include/voelund/features/FeatureGraph.hpp
- src/application/CadSession.cpp
- src/application/CadSessionCreation.cpp
- src/application/CadSessionEvaluationDetail.hpp
- src/application/CadSessionPersistence.cpp
- src/application/CadSessionSketchProfiles.cpp
- src/application/CadSessionTransactions.cpp
- src/features/CMakeLists.txt
- src/features/FeatureDependencies.cpp
- src/features/FeatureGraph.cpp
- tests/CMakeLists.txt
- tests/desktop/FeatureStatusTests.cpp
- tests/integration/FeatureDagTests.cpp
