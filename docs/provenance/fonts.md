# Eingebettete Schriften

Festgelegt in [ADR 0012](../adr/0012-ui-shell-concept.md). Die Schriften werden
unverändert als Qt-Ressource (`resources/fonts/`) in `voelund_desktop` eingebettet.

| Datei | Quelle | Commit | Lizenz | SHA-256 |
|---|---|---|---|---|
| `AlbertSans-Variable.ttf` | `google/fonts`, `ofl/albertsans/AlbertSans[wght].ttf` | `fffdadf0f0c9cc1ec8b407063424a8bfbee05611` | SIL OFL 1.1 (`AlbertSans-OFL.txt`) | `8fe5d4cf5822d7096d4d17ad781c90f97c745ac13a22be619db74966fba45fda` |
| `JetBrainsMono-Variable.ttf` | `google/fonts`, `ofl/jetbrainsmono/JetBrainsMono[wght].ttf` | `6e4b84c976cadb3c49a40fd9a1c203e4f7fcf2da` | SIL OFL 1.1 (`JetBrainsMono-OFL.txt`) | `48715a42ec242c21e9f02692891e147d022299a52e48d5e413e1a942193ffeda` |

Die OFL erlaubt Einbetten und Weitergabe mit der Software. Die Lizenztexte liegen neben den
Dateien und werden bei der Paketierung (L02) mit ausgeliefert. Reserved Font Names werden
nicht verwendet, weil die Dateien nicht verändert werden.
