# 12 – v1-Abrundung (N01–N16)

**Nutzerentscheidung 21.10.2026.** Vor Handbuch und Tutorials (L09) und Releasekandidat (L10)
schließt die v1 die Lücken, die ein mechanisches Teile-CAD üblicherweise hat. Grundlage war
eine Bestandsaufnahme nach L08: umgesetzte Befehle, dokumentierte Grenzen und Pläne. Der
Nutzer hat alle vorgeschlagenen Bereiche gewählt. Baugruppen, Zeichnungen, Blech, Freiform,
Vault, CAM und FEM bleiben G5 (Kapitel 07, 11).

Die Felder jedes Pakets (Schritte, Abnahme, Grenze) stehen in `packets.json`. Hier stehen
Reihenfolge und Arbeitsregeln.

## Reihenfolge

| Schritt | Paket | Inhalt |
|---|---|---|
| 1 | N16 | Programmsymbol |
| 2 | N02 | Zuletzt geöffnet, Kopieren/Einfügen, Mehrfachauswahl |
| 3 | N01 | Ausdrücke (`=A*2`) und Parameter in allen Maßfeldern |
| 4 | N03 | STL-/3MF-Export |
| 5 | N04 | IGES-Export, DXF-Export aus Skizzen |
| 6 | N05 | Schnittansicht |
| 7 | N06 | Darstellungsstile, Farben pro Körper |
| 8 | N07 | Material, Masse, Schwerpunkt |
| 9 | N08 | Kollisionsprüfung, Messen an Punkten |
| 10 | N09 | Bohrungsvarianten, Gewinde, Bohrungen an Skizzenpunkten |
| 11 | N10 | Fase-Varianten, Verjüngung, Drehen bis Fläche |
| 12 | N11 | Feature-Muster, Raster, Muster entlang Kurve |
| 13 | N12 | Körper teilen, Rippe |
| 14 | N13 | Direktbearbeitung, erweiterte Bezugsgeometrie |
| 15 | N14 | Skizzenkurven vervollständigen |
| 16 | N15 | Text in Skizzen, DXF-Import, Schnittkurven |

Die Reihenfolge stellt Kleines und Grundlagen (Ausdrücke vor den neuen Maßfeldern) nach vorn.
Abhängigkeiten aus `packets.json` gehen vor.

## Arbeitsregeln

- **Wie bisher je Paket:** Code, Tests und Nachweis `evidence/N??.md`. Abgenommen wird nach
  grünem CI-Lauf, danach Fast-Forward von `main`.
- **Nutzerfragen vor dem Bau:** Neue Konzepte, neue Bedienwege und Schemaänderungen legt der
  Agent dem Nutzer vor dem Bau als kurze Entscheidung vor. Beispiele sind der Gewindetyp,
  Materialliste, Farbe im Dokument und der Ausdrucksumfang.
- **Oberflächenläufe:** auf Fedora (virtuelles Wayland) oder in der Windows Sandbox, nie auf
  dem Windows-Desktop des Nutzers.
- **Sichtprüfung:** Jedes Paket mit neuer Oberfläche bekommt eine eigene Sichtprüfung nach
  A-VIS-01 mit angesehenen Bildern.
