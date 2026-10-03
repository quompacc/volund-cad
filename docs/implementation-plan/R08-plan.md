# R08 – Schnittbildung und verschachtelte Regionen

19.09.2026. Voraussetzung R07 abgenommen. Zwei Teilblöcke gemäß Kapitel 03,
weil analytische Schnittbildung und Containment getrennte Algorithmen sind.

## R08a – Analytische Schnittbildung

Abgenommen: 94/94 Windows, 91/91 FreeBSD, 385 identische Quellhashes.

Lesende Features-API splitSketchIntersections; keine Dokumentmutation.
V1-Linien/Arcs/Kreise/Ellipsen und Rechteckseiten liefern Schnittpunkte
sowie gerichtete Quellparameterintervalle. Konstruktion ausgeschlossen.
Herkunft bleibt Element-ID plus Rechteckseite, nicht Index in einer OCCT-Exploration.
Kreuzung, Tangente und Endpunktkontakt werden klassifiziert.
Überlappung und innerhalb der Toleranz nicht eindeutig trennbare Intervalle
werden mit Element-IDs abgewiesen; ungültige Ergebnisse liefern keine Intervalle.
[Näheres und Plattformprüfung](evidence/R08a.md).

## R08b – Regionsbildung und Containment

Abgenommen am 19.09.2026: 95/95 Windows, 92/92 FreeBSD, 390 identische Quellhashes. [Nachweis](evidence/R08b.md).

Umgesetzt: Intervalle in planare Halbkantentopologie übernehmen, zyklische
Randläufe und Orientierung bilden, Außen-/Innenkonturen zuordnen. Für
Tangentialfälle/überlappende Grenzen ausdrückliche Regel; keine zufällige
Zusammenführung. Periodische Naht ist Parametrisierungsdetail.
Zwei konzentrische Kreise: Ring und ausdrücklich separat auswählbare Innenregion.
Drei geschachtelte Kreise, getrennte Konturen und Selbstschnitte klassifizieren.
Flächen-/Orientierungsnachweise auf beiden OCCT-Versionen.

R08 liefert abgeleitete Regionen; produktive dauerhafte Regionidentität und
Migrationsregeln folgen R09, verlässlicher Face-/Extrusionsweg R10.
Bisherige Profilauswahl nicht still auf andere Gebiete umbiegen.
R08 mit Ring-/Containment-Abnahme abgeschlossen. Nächster Schritt R09.
