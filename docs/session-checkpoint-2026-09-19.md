# Fortsetzung – 19. September 2026

Ausgangspunkt: Tagesabschluss-Commit 6975aa7; Arbeitsbaum war sauber.
R07 und R08 vollständig abgenommen. Nächster Schritt R09 (Regionidentität/Profilauswahl).
Vertrag und Prüfumfang: implementation-plan/evidence/R07.md.
Nächster Block: R09 gemäß Kapitel 03/packets.json.

FreeBSD ist nach Nutzerhinweis eingeschaltet. Dauerhafte Transfer-/Testfreigabe
aus Kapitel 10 bleibt gültig, Host/Schlüssel wie Checkpoint vom 18.09.
Windows build/p02-qt-final, FreeBSD freebsd-clang-lean-occt mit OCCT 7.9.3.
Richtige Anwendung: apps/voelund-cad/voelund-cad.exe, nicht UI-Preview.

Neue Quellen: SketchContourGraph.hpp, SketchContourGraph.cpp,
SketchContourGeometry.hpp/.cpp und SketchContourGraphTests.cpp.
Geometrische Traversierung; gerichtete Profile; Diagnosen mit Element-IDs.
Keine Änderung von Dateischema oder Referenzherkunft, keine R08/R09-Vorwegnahme.

Windows 93/93 (115,66 s), FreeBSD 90/90 (34,84 s), 380 identische Quellhashes.
Zwölf Quell-/Testdateien gegenüber R06d. Zwei Reparaturfixtures nutzen nun
Konstruktionshilfslinien, damit die neue Verzweigungsprüfung fachlich greift.
Plan: 121 Pakete, 32 done, 88 planned, A02 needs-design; 89 offen/60 bis G4.
Alle Build-/Testprozesse beendet. Kein Commit/Push in dieser Sitzung.

R08: Kapitel 03, K07 und A-REG-01 lesen. Graph liefert bewusst nur Konturen;
allgemeine Schnittbildung/Teilung, Selbstschnitte, periodische Berührungen und
Containment fehlen noch. R08a Schnittbildung und R08b Verschachtelung trennen,
wenn erforderlich. R09 bleibt für stabile Regionidentität zuständig.

## Aktuelle Fortsetzung nach R08a

R08a: SketchIntersections.hpp mit Schnittpunkten/Kontaktart und Quellintervallen;
SketchIntersectionCurves erstellt exakte Geom2d-V1-Kurven. Überlappungen/unklare
Intervalle liefern Diagnosen und keine konsumierbaren Teilstücke. Periodische
Naht und Bogen-Originaldomäne explizit. Noch nicht in Profilauswahl/Extrusion eingebunden.
Windows 94/94 (121,71 s), FreeBSD 91/91 (35,46 s), 385 identische Quellhashes,
fünf gleiche Prüfausgaben. Artefakte unter evidence/R08a-artifacts.
Plan: 32 done, R08 in-progress, 87 planned, A02 needs-design (121 insgesamt).
R08b soll Halbkantentopologie, Orientierung, Containment und Ring/Innenregion
liefern. Anschließend R09 Identität und R10 produktiver Face-Weg.
Alle Prüfläufe beendet; kein Commit/Push. R07/R08a uncommittet.

## Aktuell nach R08b – nächster Schritt R09

R08b: SketchRegions.hpp, Halbkantentopologie und Containment mit unmittelbaren
Innenkonturen. Analytische Flächen, temporäre Klassifikationsfaces, Ring/Innenfläche
getrennt; keine automatische Auswahl. Tangentialkontakte, offene/Brückenränder,
Überlappungen werden explizit abgewiesen. Glatte Bogenanschlüsse korrigieren R08a.
Windows 95/95 (119,77 s), FreeBSD 92/92 (35,79 s), 390 gleiche Quellhashes,
fünf gleiche Prüfausgaben, acht Quell-/Testdateien gegenüber R08a.
Plan: 33 done, 87 planned, A02 needs-design; 88 offen/59 bis G4.
Alle Prüfläufe beendet. R07/R08 uncommittet; kein Push.

R09 zuerst: K02/K06/K07, SubshapeRef-Vertrag und heutige CadSessionSketchProfiles
lesen. Bisherige min-Element-ID-Profile nicht still in neue Zellen umdeuten.
SketchRegionResult-Indizes sind temporär! Stabile Grenzbezüge, Maßänderungen,
Split/Merge/Migration und explizite Auswahl definieren. R10 bleibt für den
produktiven Face-/Innenwire-Weg zuständig. Keine alte Extrusionsabsicht erraten.
