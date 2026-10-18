# Pakete und Dateizuordnung (L02)

Festgelegt durch den Nutzer am 17.10.2026. Es gibt kein neues Build-Werkzeug: gepackt wird mit
CMakes `install` und CPack.

## Windows: portabler Ordner (ZIP)

```
cmake --preset windows-msvc-release
cmake --build --preset windows-msvc-release
cd build/windows-msvc-release && cpack           # voelund-cad-0.1.0-windows-x64.zip
```

- **Inhalt:**
  - `bin/voelund-cad.exe`;
  - genau die DLLs, die der Abhängigkeitsgraph der EXE auflöst (Qt, OpenCASCADE, FreeType und
    deren Abhängigkeiten), ohne Windows-System-DLLs;
  - `bin/platforms/qwindows.dll`;
  - die MSVC-Laufzeit (`InstallRequiredSystemLibraries`).
- **Dateizuordnung pro Nutzer, ohne Administratorrechte:**
  - setzen: `powershell -ExecutionPolicy Bypass -File register-file-type.ps1`;
  - entfernen: `unregister-file-type.ps1`.

  Eingetragen wird `HKCU\Software\Classes\.volund` → `Voelund.Document`, Befehl
  `"<ordner>\bin\voelund-cad.exe" "%1"`. Das Entfernen löscht nur eine Zuordnung, die auf
  diese Kopie zeigt.

## Linux: Tarball

```
cmake --preset linux-desktop-release
cmake --build --preset linux-desktop-release
cd build/linux-desktop-release && cpack          # voelund-cad-0.1.0-Linux-x86_64.tar.gz
```

- **Inhalt:**
  - `bin/voelund-cad`;
  - die eigenen OpenCASCADE-Bibliotheken unter `lib64/` (RUNPATH `$ORIGIN/../lib64`);
  - die Desktop-Datei und der MIME-Typ `application/x-voelund` unter `share/`.

  Qt kommt vom System, siehe `tools/setup-linux.sh`.
- **Zuordnung pro Nutzer, ohne root:**
  - setzen: `sh install-user.sh`. Das Skript schreibt die Desktop-Datei mit absolutem `Exec`
    auf diese Kopie und den MIME-Typ nach `${XDG_DATA_HOME:-~/.local/share}` und macht Völund
    zum Standardprogramm für `.volund`;
  - entfernen: `sh uninstall-user.sh`, wieder nur für diese Kopie.

## Upgrade und Deinstallation (L03)

- **Getrennte Ablage:** Programmordner und Nutzerdaten liegen getrennt.
  - Einstellungen und Wiederherstellungsdaten liegen im Nutzerprofil
    (`QSettings` bzw. `<AppLocalData>/recovery`, angezeigt in „Über Völund…“).
  - Dokumente liegen dort, wo der Nutzer sie speichert, nie im Paketordner.
- **Upgrade:** Neue Version entpacken; den alten Ordner ersetzen oder daneben behalten.
  Einstellungen und Wiederherstellung bleiben. Danach das Zuordnungsskript aus dem neuen Ordner
  ausführen, damit `.volund` die neue Version öffnet.
- **Deinstallation:**
  - zuerst `unregister-file-type.ps1` bzw. `sh uninstall-user.sh`;
  - dann den Programmordner löschen.

  Dokumente, Einstellungen und Wiederherstellungsdaten bleiben erhalten.
- **Ältere Dokumente:**
  - Völund liest das eigene Format bis zum aktuellen Schema (siehe „Über Völund…“) und die
    älteren Textformate VOLUND 1–16;
  - gespeichert wird immer im aktuellen Schema.
- **Neuere Dokumente** einer späteren Version lehnt Völund ab, mit dem Hinweis, dass die Datei
  aus einer neueren Version stammt. Eine automatische Rückwärtskompatibilität gibt es nicht.

## Prüfen ohne Fenster

```
voelund-cad --version                                   # Völund CAD 0.1.0 (<revision>)
voelund-cad --check-document <datei> [--resave <ziel>]   # 0 geöffnet, 2 abgelehnt, 3 Speichern fehlgeschlagen
```

## Befehlszeile

- Das erste Dokument wird geöffnet; der Pfad darf Umlaute und Leerzeichen enthalten.
- Ein Fenster hält ein Dokument. Weitere Dokumente werden nicht geöffnet, sondern in der
  Statuszeile genannt („Ein Dokument je Fenster – nicht geöffnet: …“).

## Prüfungen

| Test | Prüft |
|---|---|
| `platform.package_runtime` | Installation in ein frisches Präfix, alle Bibliotheken im Paket oder im System, nichts aus `.tools` oder Build-Bäumen; eine entfernte OpenCASCADE-Bibliothek wird mit Namen gemeldet; Zuordnung setzen und entfernen (Windows in einem eigenen Test-Registrierungsschlüssel, Linux mit privatem `XDG_DATA_HOME`) |
| `desktop.file_association` (Linux) | MIME-Typ und Standardprogramm über `xdg-mime`; das Programm aus der registrierten `Exec`-Zeile öffnet `Prüf Ordner/Teil mit Ä.volund` aus einem Paketordner `Völund Paket` |
| `desktop.product_arguments` | erstes Dokument unter Umlaut-/Leerzeichenpfad offen, weitere in der Statuszeile genannt |
| `desktop.installed_product` | installiertes Programm startet und öffnet ein Dokument |
| `platform.package_runtime` (L03-Teil) | installiertes Programm: Version = Paketversion; VOLUND 1 und 16 öffnen, neu speichern, wieder öffnen; „VOLUND 99“ abgelehnt mit „neueren Völund-Version“; erneute Installation und Entfernen des Ordners ändern ein Nutzerdokument nicht |
| `platform.version` | `voelund-cad --version` ohne Fenster |
| `desktop.about_dialog` | „Über Völund…“ letzter Menüeintrag; Version, Revision, Schema, Bibliotheken; „Diagnose kopieren“ |

Unter Windows prüft niemand automatisch, dass ein Doppelklick im Explorer das Dokument öffnet.
Auf der Entwicklungsmaschine laufen keine GUI-Tests, und es gibt keinen Windows-Runner. Das bleibt
eine Nutzerprüfung.
