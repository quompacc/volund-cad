# LIZENZ – Völund unter GPL-3.0-or-later

**Status:** Code und Prüfungen vom 22.10.2026; Abnahme nach grünem CI-Lauf.

## Entscheidung (Projektinhaber, 22.10.2026)

- Völund CAD steht unter der **GPL-3.0-or-later**.
- Copyright-Zeile: „Copyright (C) 2026 Eduard Hart“.
- Das löst den Stand aus L04 ab („eigene Lizenz vorerst offen“, `LicenseRef-Voelund-undecided`).
- Nur Lizenzangaben; das Produktverhalten ändert sich nicht.

## Änderung

1. **`LICENSE`** im Wurzelverzeichnis: der unveränderte Volltext der GPL-3.0.
   - SHA-256 `3972dc9744f6499f0f9b2dbf76696f2ae7ad8af9b23dde66d6af86c9dfb36986`, 35 149 Byte.
     Das ist die Prüfsumme von <https://www.gnu.org/licenses/gpl-3.0.txt>.
   - gnu.org war vom Arbeitsplatz aus nicht erreichbar. Die Datei stammt aus der
     Fedora-Paketkopie `/usr/share/licenses/cockpit-bridge/GPL-3.0.txt` mit genau dieser
     Prüfsumme.
   - `.gitattributes` schützt sie vor Zeilenende-Umwandlung (`LICENSE -text`).
2. **SPDX-Kopf** `SPDX-License-Identifier: GPL-3.0-or-later` und die Copyright-Zeile:
   - in 827 eigenen Dateien unter `src/`, `include/`, `apps/`, `tests/`, `tools/`, `cmake/`;
   - Dateitypen C++, Header, `.rc`, CMake, Shell, PowerShell, Python;
   - die Zeilen stehen ganz oben, nach einer Shebang-Zeile.
   - Nicht angefasst: `third_party/` (PlaneGCS bleibt LGPL-2.1-or-later) und `spikes/`.
   - In `src/viewport/QtOcctGlContext.cpp` bleibt der MIT-Hinweis von Kirill Gavrilov
     unverändert stehen; die GPL-Zeilen kommen davor dazu.
3. **Prüfung** `architecture.source_licenses` (`tools/cmake/CheckLicenseHeaders.cmake`):
   - Sie wird rot, wenn eine eigene Datei dieser Typen in den genannten Verzeichnissen keine
     SPDX-Zeile unter ihren ersten fünf Zeilen hat, und nennt die Datei.
   - `architecture.source_licenses_regression` belegt Erfolg (auch nach Shebang, `third_party`
     ausgenommen), Fehler mit Dateinamen und den leeren Baum.
4. **Paket:**
   - `packaging/Components.cmake` führt die Komponente `voelund` mit `GPL-3.0-or-later` und
     dem Hinweis `licenses/voelund/LICENSE`.
   - Das Paket enthält die Datei.
   - `platform.package_runtime` prüft, dass sie im installierten Paket liegt und Byte für Byte
     dem Repository entspricht. Die Hinweisprüfung (L04) verlangt sie als Hinweis der
     Komponente.
   - `THIRD-PARTY-NOTICES.md` nennt die Produktlizenz statt „noch nicht festgelegt“.
5. **Über-Dialog:**
   - Er zeigt den Lizenznamen, „ohne jede Gewährleistung“ und Verweise auf den Lizenztext
     und die Lizenzen der Komponenten. Liegen die Dateien im Paket, sind es Links.
   - Die Diagnose nennt „Lizenz: GPL-3.0-or-later“.
   - `desktop.about_dialog` prüft das.
6. **Dokumentation:**
   - README mit Abschnitt „Licence“ (Verweis auf `LICENSE` und `docs/dependencies.md`).
   - `docs/dependencies.md` mit Abschnitt „Product licence“.
   - Das Symbol-README nennt die Lizenz.

## Prüfungen

| Szenario | Nachweis |
|---|---|
| Jede eigene Quelldatei mit SPDX-Zeile | `architecture.source_licenses` |
| Fehlende Zeile wird rot und benannt; Shebang, `third_party` | `architecture.source_licenses_regression` |
| `licenses/voelund/LICENSE` im Paket, gleich dem Repository; Komponente mit Hinweis | `platform.package_runtime` |
| Über-Dialog mit Lizenz, Gewährleistungsausschluss und Verweisen | `desktop.about_dialog` |
| Quellgrenzen (600 Zeilen) trotz der neuen Kopfzeilen | `architecture.source_limits` |

## Grenzen

- Keine rechtliche Prüfung; die Herkunftsdokumente unter `docs/provenance/` gelten weiter.
- Ältere Nachweise (etwa L04) bleiben als Zeitstand unverändert.
