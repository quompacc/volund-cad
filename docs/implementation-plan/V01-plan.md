# V01 – Teilplan: Durchstich leeres Dokument → Ebene → Skizze → Extrusion

Stand 04.10.2026. Grundlage: `packets.json` (V01), S03, R09/R10, ADR 0011.

## Nutzerentscheidung vom 04.10.2026

Basisebenen werden **im Viewport** angeboten: Im Dokument liegen XY, XZ und YZ als
halbtransparente Flächen. Hover zeigt einen Rahmen, Klick wählt die Ebene, danach startet
„Skizze“ darauf (Arbeitsweise wie SolidWorks/Onshape). Es gibt keinen Ursprungsordner im
Modellbaum und keinen Ebenendialog.

Der Nutzer ist sich beim bisherigen Oberflächenstil unsicher. Jede weitere sichtbare
UI-Änderung, die über diese Entscheidung hinausgeht, wird vorher mit ihm abgestimmt.
Aufnahmen werden zur Durchsicht vorgelegt.

## Unterpakete

| Paket | Inhalt | UI |
|---|---|---|
| V01a | Basisebenen als stabile Datumsreferenzen im Dokument; Skizze ohne Körper; Extrusion einer solchen Skizze erzeugt einen neuen Körper; Persistenz, Undo/Redo, Migration | nein |
| V01b | Viewport-Ebenen nach obiger Entscheidung: Darstellung, Hover, Pick, Skizzenstart, Kamera senkrecht | ja – Aufnahmen vor Abnahme zur Durchsicht |
| V01c | Qt-Testtreiber mit echten Events für den ganzen Ablauf; CI unter Linux, lokal Windows | nein |

Abnahme laut Paket: neues Dokument → XY → Rechteck mit Innenkreis → Extrusion (Ring,
Volumen analytisch) → Distanz ändern → Undo/Redo → Speichern → Öffnen mit gleichem Volumen.
