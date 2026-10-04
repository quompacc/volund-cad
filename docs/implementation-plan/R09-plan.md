# R09 – Teilplan: Regionidentität und Profilauswahl

Stand 04.10.2026. Grundlage: K02, K06, K07, A-REG-02, [R08](evidence/R08.md),
`docs/subshape-reference-contract.md`. Umfang nach der Umstellung vom 04.10.2026
auf den Bedarf von V01 begrenzt; weitergehende Fortführungsregeln folgen nach V01.

## Ausgangslage

- `SketchProfileId` ist die kleinste Element-ID einer Konturgraph-Schleife und wird
  in Extrusionen gespeichert (natives Schema 4, Legacy ab VOLUND v5). Eine leere Liste
  bedeutet „alle geschlossenen Profile“.
- Die Extrusion wertet noch Schleifen aus; Rechteck mit Innenkreis ergibt zwei
  gefüllte Flächen und keinen Ring. Flächen mit Löchern liefert erst R10.
- R08b liefert Regionen (Außenrand, unmittelbare Löcher) mit Rückverweis auf
  Element-ID, Rechteckseite und Laufrichtung, aber ohne dauerhafte Identität.

## Entscheidungen

1. **RegionId = kanonische Randsignatur.** Ein Randstück wird als `<Element-ID>.<Teil>`
   geschrieben, Teil ist die Rechteckseite (sonst 0). Kreise, Bögen und Ellipsen
   erhalten zusätzlich `+` oder `-` für die Laufrichtung des positiv orientierten Randes
   relativ zu ihrer stets gegen den Uhrzeigersinn laufenden Parameterrichtung. Linien und
   Rechteckseiten erhalten kein Vorzeichen: Ihre Parameterrichtung hängt von der
   Zeichenrichtung ab, und ein Umdrehen dürfte nie still eine andere Zelle auswählen.
   Ein Rand ist die sortierte Menge seiner Stücke. Mehrfachvorkommen, etwa beidseits
   der Kreisnaht, werden zusammengefasst, damit ein wandernder Schnitt die Identität
   nicht ändert.
   Die RegionId ist `o:<Außenrand>` gefolgt von `;h:<Lochrand>` je unmittelbarem Loch,
   Löcher sortiert. Beispiel Rechteck 1 mit Innenkreis 2: Ring
   `o:1.0,1.1,1.2,1.3;h:2.0+`, Innenscheibe `o:2.0+`.
   - Maßänderungen ohne Topologiewechsel erhalten die Signatur.
   - Elementreihenfolge, Zeichenrichtung von Linien, Indizes und unbeteiligte Elemente
     oder Konstruktionen gehen nicht ein.
   - Die Kurvenrichtung trennt Linse und Sicheln zweier sich schneidender Kreise.
     Spiegelbildliche Zellen an einer Sehne bleiben gleich signiert und werden als
     mehrdeutig gemeldet. Ihre geometrische Unterscheidung ist ein Folgepaket nach V01.
2. **Keine stille Fortführung.** Eine gespeicherte RegionId wird nur bei exakter,
   eindeutiger Signatur aufgelöst. Andernfalls gilt sie als reparaturbedürftig, mit
   folgendem Grund:
   - `elementsRemoved`: ein referenziertes Element fehlt. Damit wählt das Löschen der
     kleinsten ID nie eine andere Region.
   - `topologyChanged`: alle Elemente sind noch vorhanden, aber Split, Merge oder ein
     neuer Schnitt ändert den Rand.
   - `ambiguous`: mehrere Zellen tragen dieselbe Signatur. Es wird nie geraten.
   - `invalidSketch`: die Regionsbildung selbst scheitert.

   Eine eindeutige Split-Fortführung ist ein Folgepaket nach V01.
3. **Migration alter Profil-IDs.** Dokumentierte Ausgangszuordnung: Eine alte
   Profil-ID bezeichnet die Schleife, deren kleinste Element-ID sie ist. Sie wird auf
   die Region abgebildet, deren Außenrand genau die Elemente dieser Schleife trägt,
   und auf alle darin verschachtelten Regionen. Die Vereinigung ist genau die früher
   extrudierte gefüllte Fläche, die Geometrie bleibt also erhalten. Ist keine
   eindeutige Abbildung möglich, wird die Extrusion reparaturbedürftig und behält
   die alte ID zur Anzeige. Eine leere Liste bleibt „alle Regionen“.
4. **Persistenz.** RegionIds werden als kanonischer ASCII-Text gespeichert, im nativen
   Schema 5. Schema 1–4 wird gelesen und beim Öffnen migriert. Die
   Signaturgrammatik wird beim Lesen strikt geprüft.

## Unterpakete

| Paket | Inhalt | Abnahme |
|---|---|---|
| R09a | `document::SketchRegionId`, `identifySketchRegions`, `resolveSketchRegion`, `legacyProfileRegions` in features; keine Persistenz, keine UI | Unit-Tests: Umordnung/ID-Permutation, Radiusänderung, irrelevante Konstruktion/Elemente, Löschen der kleinsten ID, Split durch Linie, sich schneidende Kreise, Mehrdeutigkeit, Migrationsabbildung Rechteck+Kreis |
| R09b | `ExtrusionParameters` mit RegionIds und optionalen Altprofilen, Schema 5, Legacy-Import, Migration beim Öffnen, Reparaturgrund | Rundlauf Schema 5, Schema-4-Datei wird migriert, unabbildbares Profil wird reparaturbedürftig statt umgedeutet |
| R09c | Auswahl per Pick → RegionId, Mehrfachauswahl, ungültige Referenz in der Application-Projektion, Extrusion über RegionIds (Auswertung bis R10 über abgebildete Schleifen) | A-REG-02 als Integrationstest, CI grün, Windows lokal grün |

R10 ersetzt anschließend die Schleifenauswertung durch Faces mit Innenwires; V01
nutzt beides.
