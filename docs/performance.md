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
