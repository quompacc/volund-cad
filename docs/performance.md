# Performance-Budgets (L06, A-PERF-01)

Vereinbart mit dem Nutzer am 18.10.2026.

- **Referenzrechner:** Fedora 44, Intel i7-7500U (2 Kerne, 4 Threads), 15,5 GiB RAM.
- **Build:** Release (`linux-desktop-release`).
- Ein langsamerer Rechner verschiebt die Werte, daraus folgt kein Hardwareversprechen.

## Budgets für den Arbeitsfall

| Arbeitsfall | Grenze | Gemessen am 18.10.2026 |
|---|---|---:|
| Typisches Bauteil, 100 Features (`part-100`): Feature hinzufügen | p95 ≤ 0,5 s | 83 ms |
| … einen Körper ändern | p95 ≤ 2 s | 0,65 s |
| … öffnen | ≤ 5 s | 2,9 s |
| Skizze mit 200 Elementen (`sketch-200`): Maß ändern | p95 ≤ 1 s | 0,26 s |
| … öffnen | ≤ 1 s | 0,26 s |
| … Spitzenspeicher | ≤ 200 MB | 73 MB |
| 100 Körper (`bodies-100`): einen ändern | p95 ≤ 0,5 s | 63 ms |
| Restspeicher nach 10× Öffnen/Schließen, jede Stufe | ≤ 10 MB Zuwachs | ≤ 4 MB |

## Bekannte Grenzfälle (ohne Budget)

| Fall | Gemessen | Grund |
|---|---:|---|
| 50 Aufsätze auf **einer** Fläche (`features-100`): alles neu / öffnen | 6,8 s / 6,4 s | Jede Vereinigung schneidet gegen die wachsende Plattenfläche (OCCT-Boolean). |
| Skizze mit 1000 Elementen (`sketch-1000`): Maß ändern / Speicher | 3,4 s / 689 MB | Die Diagnose des Lösers läuft über die ganze Skizze. Folgepaket L06a. |
| 500 Features auf einer Fläche (`features-500`) | Aufsatz hinzufügen p95 2,6 s; Gesamtstufe > 15 min | wie oben |

Während einer Neuberechnung ist die Oberfläche blockiert. Die Berechnung im Hintergrund ist als
Folgepaket L06b vorgemerkt.

## Messen

```
cmake --preset linux-desktop-release && cmake --build --preset linux-desktop-release
sh tools/run-perf.sh build/linux-desktop-release        # alle Stufen, 15 min je Stufe
```

- Gemessen wird auf einem ruhigen Rechner, ohne CI-Lauf und ohne parallelen Build.
- Berichte liegen in `docs/implementation-plan/evidence/L06-artifacts/`.
- Vor jedem Release wird die Messung wiederholt; jede Überschreitung eines Budgets muss geklärt
  werden.
- CTest hält nur die kleinen Stufen lauffähig (`perf.smoke.*`) und misst keine Zeiten.

## Undo-Stapel (UNDO-MESSUNG, 23.10.2026)

Die Historie hält je Schritt einen vollständigen OCAF-Dokumentstand (D12c). Gemessen ohne
Änderung daran: jeder Schritt ist eine übernommene Änderung des ersten Quaders, danach alle
Schritte zurück und wieder vor. Messung: Release-Build, ruhiger Referenzrechner, kein CI-Lauf
(`sh tools/run-undo-bench.sh`). Rohwerte: `docs/implementation-plan/evidence/UNDO-artifacts/`.

| Modell | Features | Datei | Ändern (Median) | Undo (Median) | Speicher nach 10 / 100 / 500 Schritten |
|---|---:|---:|---:|---:|---:|
| klein: ein Quader | 1 | 1 KiB | 2,2 ms | 2,1 ms | 0 / 0 / 5,4 MiB |
| mittel: 25 Körper aus Quader und Zylinder | 49 | 20 KiB | 20 ms | 20 ms | 0 / 2,1 / 32 MiB |
| groß: mittel + eingebettetes STEP-Teil (NIST STC 08) | 50 | 300 KiB | 0,60 s | 0,34 s | 0,6 / 29 / 170 MiB |

- **Speicher:** Je Schritt etwa die Größe des gespeicherten Dokuments (rund 1,1-fach), linear in
  der Zahl der Schritte. Die Historie hat keine Obergrenze. Kleine Werte unter 1 MiB gehen in
  der Seitengröße des Prozessspeichers unter.
- **Zeit:** Undo und Redo kodieren den ganzen Stand neu und werten neu aus. Das kostet rund
  1 ms je KiB Dokument und hängt nicht von der Länge der Historie ab (gleich bei 10 und 500
  Schritten).
- **Einschätzung:**
  - Bis etwa 1 MiB Dokument (typische Bauteile, auch mit kleinem Import) ist der Stapel
    unkritisch: unter 1 s je Undo und unter 0,6 GiB nach 500 Schritten.
  - Ab etwa 2 MiB Dokument wird Undo spürbar (über 2 s, über dem Budget für Änderungen), und
    500 Schritte belegen über 1 GiB.
  - Kritisch sind große eingebettete STEP-Importe (10 MiB und mehr): rund 10 s je Undo und
    über 5 GiB nach 500 Schritten. Dafür bräuchte es eine Obergrenze der Historie oder
    Delta-Undo.
  - Delta-Undo ist nicht Teil dieser Messung (D12d-Grenzen bleiben).
