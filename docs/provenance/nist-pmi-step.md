# Fremdreferenzen (Q01)

## NIST MBE PMI Testmodelle

Testdaten, kein Produktbestandteil. Unverändert unter `tests/differential/nist/` abgelegt. Die
Dateien sind byte-genau eingecheckt (`.gitattributes`: `-text`).

| Angabe | Wert |
|---|---|
| Herausgeber | National Institute of Standards and Technology (NIST), MBE PMI Validation and Conformance Testing Project |
| Quelle | <https://www.nist.gov/document/nist-pmi-step-files>, Übersicht: <https://www.nist.gov/el/systems-integration-division-73400/mbe-pmi-validation-and-conformance-testing/download-free-cad> |
| Abruf | 17.10.2026, `NIST-PMI-STEP-Files.zip` (13 976 599 Bytes, SHA-256 `8fa78429e6d8d9b0d7681d223b6aa9ec98c3772185c55b1a0e3679b21c181911`) |
| Nutzungsbedingungen | Laut NIST „can be used without any restrictions“. NIST bittet um Nennung als Quelle. Die Nutzung bedeutet keine Empfehlung oder Billigung durch NIST; das NIST-Logo wird nicht verwendet. Es gilt der [NIST-Disclaimer](https://www.nist.gov/disclaimer). |
| Hinweis der Quelle | Laut README sind es keine fehlerfreien Referenzdateien: Syntaxfehler sind möglich, und das schreibende CAD-System ist entfernt. |

| Datei | SHA-256 |
|---|---|
| `nist_ctc_01_asme1_ap242-e1.stp` | `85a5752da05f53c456ca3a9e038c90358e1d5a3141d1f0d6e5f0970f2356e821` |
| `nist_ctc_02_asme1_ap242-e2.stp` | `99a0a2079ddeb64d05c2432cbe931fa17c89da60d8b0bcb0b11bd0ed94fa2e68` |
| `nist_ctc_03_asme1_ap242-e2.stp` | `196b665776e759282f80fc8fb27d7bceb995df77cf78b7ce48347535a4d6cb5f` |
| `nist_ctc_04_asme1_ap242-e1.stp` | `20b43b54ce25d4ed17cff794084c406e831c687f5b62471d3371eef33669e355` |
| `nist_ftc_06_asme1_ap242-e2.stp` | `b87afb7f25adc0d44d12e182209d50f0b085b533c0742fb8ffd58f4c87403447` |
| `nist_ftc_07_asme1_ap242-e2.stp` | `aa8115c12e12a99cc439477cbfd222916f9ec629fbb3c9002b18e0c264c9a892` |
| `nist_stc_08_asme1_ap242-e3.stp` | `64f24b914910ce8e20c9e4bd59637b4fd155715c7c7b51689466534ed7db1442` |

**Referenzwerte.**

- `tests/differential/nist/reference.txt` enthält die *Geometric Validation Properties*, die das
  schreibende CAD-System in die AP242-Dateien eingetragen hat: Volumen, benetzte Fläche,
  Schwerpunkt.
- Die Werte wurden mit Entitätsnummer abgeschrieben. Zollwerte (FTC-07) sind mit genau
  25,4 mm/in umgerechnet.
- STC-09 ist nicht aufgenommen. Seine Datei nennt `CUBIC MILLIMETRE`, die Werte sind aber
  Zoll. Umgerechnet weichen sie 1,3–1,5e-4 von der Geometrie ab, die FTC-09 (AP242 und AP203)
  und STC-09 übereinstimmend ergeben.

## FreeCAD-Vergleichsteile

| Angabe | Wert |
|---|---|
| Werkzeug | FreeCAD 1.1.4 (LGPL-2.0+), lokal installiert. Nur der Erzeuger der Zahlen; nichts davon geht in den Build. |
| Skript | `tools/reference-freecad/make_references.py`, Aufruf `freecadcmd tools/reference-freecad/make_references.py <tabelle>` |
| Ergebnis | `tests/differential/freecad/reference.txt` (nur Zahlen) |
| Bekannte Abweichungen | `tests/differential/freecad/known-differences.txt` |

FreeCAD rechnet ebenfalls mit OpenCASCADE. Die Teile prüfen deshalb Völunds Feature-Logik, nicht
den Geometriekern. Kernunabhängig sind die NIST-Teile.
