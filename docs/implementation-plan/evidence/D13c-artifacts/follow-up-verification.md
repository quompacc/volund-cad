# Ergänzung zum ersten Prüfskript

Der erste Transfer umfasste zwölf ausdrücklich freigegebene Dateien aus
`d13c-source-delta.txt`. Die erste Gesamtsuite scheiterte auf beiden Plattformen
nur an `features.body_identity`: Der Test bearbeitete den nun binären
Produktwriter-Output als Legacy-Text.

Nach gesonderter Freigabe wurde zusätzlich
`tests/integration/BodyIdentityEvaluationTests.cpp` übertragen. Der Test nutzt
jetzt eine ausdrückliche Legacy-Fixture und prüft zusätzlich den nativen Rundlauf.
Danach auf FreeBSD im isolierten Prüfbaum:

```sh
cmake --build --preset freebsd-clang-lean-occt -j4
env -u DISPLAY XDG_RUNTIME_DIR=/var/run/xdg/eduard WAYLAND_DISPLAY=wayland-1 QT_QPA_PLATFORM=wayland ctest --preset freebsd-clang-lean-occt --output-on-failure --output-junit "$PWD/d13c-ctest.xml"
```

Windows wurde ebenfalls erneut gebaut und vollständig geprüft. Zwei überzählige
Leerzeilen am Dateiende in tests/CMakeLists.txt und NumericValidationTests.cpp
wurden abschließend entfernt und im freigegebenen Umfang angeglichen. Beide
Plattformen erneut gebaut, document.numeric_validation nochmals bestanden.
Die vollständigen Suites wurden wegen dieser reinen Leerzeilenänderung nicht
nochmals wiederholt. Finales Manifest: 318 Dateien auf beiden Plattformen identisch.
