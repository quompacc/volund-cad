# Befehlsresultat und Transaktionsvertrag (D04)

Status: verbindlicher, nativ geprüfter Schnittstellenentwurf, 13.09.2026.
Die produktiven CadSession-Aufrufer verwenden ihn erst nach D05/D06.

## Eigentum und Commitgrenze

`CommandResult` und `CommandTransaction` gehören application. Das Dokument bleibt
alleiniger Eigentümer von Parametern, IDs und History; features besitzt die
Auswertungs-Datentransfertypen. Es gibt keine neue Qt-, OCCT- oder andere
Bibliotheksabhängigkeit des Vertrags. Das skalare In-memory-Modell existiert nur
in CommandContractTests.cpp und ist ausdrücklich kein weiterer Dokumentkern.

CommandTransaction besitzt genau einen Backend-Adapter exklusiv und ist weder
kopierbar noch verschiebbar. Der D05/D06-Adapter in application kapselt die
DocumentTransaction aus document; document darf dafür nicht application
inkludieren. Der Adapter hält Kandidat und Rücknahmedaten, einschließlich
anwendungsseitiger Auswahl und temporärer Bearbeitungszustände. Er darf nicht
zwei autoritative Dokumente synchronisieren: der Kandidat ist unveröffentlicht.

Vor finish muss der Adapter den Kandidaten einfrieren. Die EvaluationReport-
Einträge stammen vollständig aus genau diesem Kandidaten; Änderungen danach
verlangen erneute Auswertung. D04 enthält keinen asynchronen Evaluator und keinen
Versionsvergleich. D05 führt den Dokumentzustandsvertrag ein, D06 bindet dessen
Stand an den Adapter; spätere asynchrone Auswertung benötigt explizite Revisionen
und verwirft überholte Ergebnisse. Ein frei erfundener leerer Report beweist keine
Geometriegültigkeit. Ein leerer Report ist nur bei leerer betroffener Menge gültig.

`prepareCommit(report)` darf Speicher für History und konsistente Veröffentlichung
reservieren, aber den aktiven Stand noch nicht ändern. `commitPrepared()` besitzt
eine einzige, nichtwerfende Veröffentlichungsgrenze. Parameter, IDs, History,
Auswahl und Auswertung werden gemeinsam übernommen. D05/D06 müssen diese
Backend-Voraussetzung am echten ModelDocument beweisen; ein noexcept allein
macht einen allokierenden Commit nicht sicher. Ergebnisdaten werden zuvor
vorbereitet; nach Commit ist die Rückgabe nichtwerfend verschiebbar.

Cancel, Rejected und der Destruktor einer offenen Transaktion verwerfen den
Kandidaten. Ein Fehler in prepareCommit ruft sofort rollback auf und wird erneut
geworfen. Programmier-/Infrastrukturfehler sind keine geometrischen Ablehnungen.
Der spätere Qt-Controller übersetzt sie an der Callback-Grenze. Der Backend-
Destruktor muss ebenfalls sicher sein. Nach Abschluss gibt jeder weitere
Abschlussversuch Rejected/AlreadyFinished zurück, ohne weitere Backend-Aufrufe.

## Resultate und Auswertung

| Resultat | Bedeutung |
|---|---|
| Rejected | Kein Commit; reason unterscheidet Cancelled, NoChange, InvalidInput, EvaluationFailed, IncompleteEvaluation, AlreadyFinished. NoChange/Cancelled sind normale Ergebnisse ohne Fehlermeldungspflicht. |
| Committed | Kandidat übernommen, keine Failed-/Blocked-Zustände. |
| CommittedWithErrors | Bestehendes Modell ausdrücklich mit reparierbaren Folgefehlern übernommen. |

Keine implizite bool-Konvertierung. UI-Code muss die drei Fälle ausdrücklich
unterscheiden. EvaluationIssue trägt Feature-ID, stabilen Code, verständliche
Nachricht und technische Details. Codes sind sprachunabhängig; die Fixture-Codes
sind keine produktiven Kernel-Fehlercodes. Die konkrete Zuordnung folgt D06/R01.

EvaluationStatus: Valid, Dirty (noch nicht ausgewertet), Failed,
BlockedByDependency und Suppressed. Dirty verhindert jeden Commit; Failed/Blocked
führen bei NewFeature stets zur Ablehnung. Bei EditFeature erlaubt ausschließlich
ErrorDisposition::keepEditableErrors einen CommitWithErrors. Suppressed ist kein
Fehler und bedeutet keinen vorhandenen aktuellen Shape. Die fünf Zustände ersetzen
noch nicht die vorhandene zweistufige FeatureRuntimeStatus-Projektion.

## Zustandsübergänge

| Aktion | Vor Abschluss | Abschluss / History |
|---|---|---|
| NewFeature, auch Sketch+Extrude | Kandidat, beliebig viele Teiländerungen/Previews | Vollständig gültig: ein Commit und ein Undo. Fehler: alles verwerfen, keine verbrauchten Kandidaten-IDs. |
| EditFeature gültig | Bestehender Stand bleibt veröffentlicht | Ein Commit; NoChange erzeugt keinen Eintrag. |
| EditFeature mit Folgefehlern | UI zeigt Diagnose und ausdrückliche Übernahmeoption | Ohne Übernahmeentscheidung Rejected, mit ihr CommittedWithErrors. Kein stilles true. |
| Cancel / Exception | Teiländerungen nur im Kandidaten | Parameter, IDs, Auswahl, Solve-/Referenzzustand und History des Ausgangsstands bleiben erhalten. |
| Undo / Redo | Vollständigen Zielzustand vorbereiten | History-Cursor wechseln, keinen weiteren Befehl aufzeichnen. Auswertung passend zum Zielstand veröffentlichen. |
| Save / Save As | Snapshot validieren und atomar ersetzen (D03) | Erst nach Dateiersatz Clean/Pfad übernehmen, kein Undo-Eintrag. Savefehler verändert beides nicht. |

Dirty wird am gespeicherten Inhaltszustand erkannt. Undo zum Save-Zustand ist
clean; Auswahl/Kamera sind keine Dokumentänderung. Revision für veraltete
Auswertungen und gespeicherte Inhaltsidentität dürfen nicht verwechselt werden:
ein monotoner Veröffentlichungszähler wird bei Undo nicht zurückgesetzt.

## Migration und konkret geprüfter Altbestand

- D05 ersetzt recordChange je Teilmutator durch eine äußere DocumentTransaction.
  Die derzeit allokierenden noexcept-Pfade currentState/restoreState/undo/redo
  werden dabei bereinigt. Zwei implizite History-Einträge für Sketch+Extrude sind
  kein Präzedenzfall für den Zielvertrag.
- D06 ersetzt mutationsbezogene bool-/FeatureId-Rückgaben an der Befehlsgrenze
  durch Resultate plus explizite erzeugte IDs in der Ergebnisprojektion. Die
  CadSession-Setter mischen derzeit "Parameter geändert" und "Auswertung gültig":
  setSketchParameters liefert recompute(), mehrere andere Setter ignorieren es.
  Deshalb ist eine mechanische Konvertierung true→Committed falsch.
- canUndo/canRedo/canAddCylinder und reine Auswahlprüfungen dürfen bool bleiben.
  Private low-level Mutatoren dürfen "Inhalt geändert" zurückmelden, entscheiden
  aber nicht über die öffentliche Commitdiagnose.
- Die spätere UI listet bei CommittedWithErrors betroffene Features. D04 fügt
  keine generischen Bestätigungsdialoge und keine neue sichtbare Bedienung hinzu.
- R01 führt echte DAG-Zustände ein. Die bisherige lineare Fehlerfortpflanzung wird
  durch D04 weder umbenannt noch als korrekter Abhängigkeitsnachweis ausgegeben.

## Prüfgrenze

Zwei native Fixtures prüfen die echte CommandTransaction gegen einen kleinen,
rein testinternen Publication-Adapter: zusammengesetzter Commit/Undo/Redo/Save,
Cancel/Destruktor, Prepare-Exception, doppelter Abschluss und die Fehlerpolitik.
Das ist ein ausführbarer Vertrag, kein Nachweis, dass das bestehende
ModelDocument bereits atomare Benutzertransaktionen besitzt. Dieser Nachweis
ist D05/D06 vorbehalten. Keine sichtbare Änderung, daher keine neue Bildabnahme.

## Stand nach D05

Die Dokumentgrenze ist in DocumentTransaction implementiert und nativ geprüft;
[Evidenz und genaue Grenzen](implementation-plan/evidence/D05.md). Der D04-Adapter
und die CadSession-Migration bleiben D06. D05 veröffentlicht ausschließlich
Dokumentzustand/History; Auswahl und Auswertung sind weiterhin Anwendungsverantwortung.


## Stand nach D06

CadSession nutzt jetzt DocumentTransaction für Featureerzeugung und Parameteränderungen;
[Evidenz D06](implementation-plan/evidence/D06.md). Die gemeinsame Grenze bereitet
Auswertung und Auswahl vor dem Commit vor. Die vollständige CommandResult-/Fehler-
und UI-Migration wird im konkreten D07-Paket abgeschlossen, einschließlich der
bisher breiten Exception-Übersetzung in FeatureGraph. Frühere Hinweise auf D06
als alleinigen Abschluss dieser Migration waren zu knapp; die dreistufige
Resultatpolitik ist noch nicht produktiv abgenommen.

## D07-Zwischenstand

Die produktiven Erzeugungswege und SketchEditor-Bestätigung konsumieren jetzt
CommandResult. Ein SessionDocumentBackend verbindet D04 mit der Dokumentgrenze;
neue ungültige Features werden verworfen. Der nächste Schritt migriert die noch
vorhandenen Legacy-Setter und entfernt die Übergangsadapter. Die Gesamtpolitik
ist weiterhin nicht abgenommen: Ausnahmeübersetzung, stale-Projektion und
ausdrückliche Fehlerübernahme in der UI fehlen noch. Details in
[D07b](implementation-plan/evidence/D07b.md).

D07c migriert auch Parameter-/Placement-Aufrufer auf typisierte Resultate mit
Default-Ablehnung. Geometriefehler besitzen eine engere Ausnahmegrenze; Speicher-
und Programmierfehler werden weitergereicht. Undo/Redo bereiten Session und
Auswertung vor Veröffentlichung vor. Verbleibende Adapter, Eingabevalidierung,
stale-Projektion und ausdrückliche UI-Fehlerübernahme stehen im
[aktuellen Restauftrag](implementation-plan/evidence/D07c.md).

D07d bindet Dokument und Auswertung an Generation und Revision. Veraltete Formen
sind nur über ausdrückliche Darstellungszugriffe verfügbar; aktuelle Geometrie-
zugriffe und davon abhängige Aktionen sind gesperrt. Parameterpanel und Editor
bestehender Skizzen bieten eine eigene, an Dokument/Entwurf gebundene Fehlerübernahme.
Save/Open/New und Undo/Redo sind gegen diese Projektion geprüft. Die verbleibende
Eingabevalidierung, Adapterentfernung und Gesamtabnahme stehen in
[D07d](implementation-plan/evidence/D07d.md); D07 bleibt in-progress.

D07e führt Eingabefehler vor der Auswertung in `invalidInput` mit eigener Diagnose
über und entfernt die öffentlichen ID-/bool-Kompatibilitätsadapter. Undo/Redo
liefern typisierte Ergebnisse einschließlich `committedWithErrors`; ihre Diagnose
entsteht vor Veröffentlichung. Read-only-Prüfungen sowie interne ModelDocument-
Mutatoren bleiben davon getrennt. Konkrete UI-Restpunkte für A-ERR-01 stehen in
[D07e](implementation-plan/evidence/D07e.md). D07 ist weiterhin nicht insgesamt abgenommen.

## D07-Fundament abgenommen

D07f schließt überholte Parameterangebote und Preview-Ausnahmegrenzen und führt
blocked als eigene Laufzeit-/Baumprojektion. D07 ist damit als Fehlerfundament
abgenommen; vollständige A-ERR-01-Produktnachweise für gemeinsame Shell, Timeline
und Export bleiben gemäß Kapitel 08 R01/U10/M15/M17/M18 zugeordnet. Die obigen
Zwischenstände sind historisch. [Abnahme und Grenzen](implementation-plan/evidence/D07f.md).
