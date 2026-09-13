# 09 – Ausführungsauftrag für Terra und andere kleinere Modelle

Das Ziel ist nicht, Terra bei jedem Auftrag die gesamte Projektarchitektur neu entwerfen zu lassen. Die Architekturverantwortung liegt im Plan und den abgeschlossenen Entscheidungspaketen. Das ausführende Modell bearbeitet eine begrenzte Änderung mit klaren Ergebnissen. Auch dann bleibt Review notwendig, besonders für numerische Geometrie, Persistenz und Referenzhistory.

Die offizielle [Beschreibung von GPT-5.6 Terra](https://developers.openai.com/api/docs/models/gpt-5.6-terra) macht keine projektspezifische Korrektheitszusage. Dieser Ablauf hängt deshalb nicht von behaupteten Modellfähigkeiten ab, sondern von kleinen Aufträgen und messbaren Nachweisen.

## Auftrag zum direkten Kopieren: erstes Paket

Die folgenden Vorlagen gelten für eine beauftragte Umsetzung. Den tatsächlichen Stand zuerst im Manifest prüfen: Bereits abgeschlossene Pakete nicht erneut ausführen. Standard ist die selbstständige Fortsetzung nach Kapitel 10; ein ausdrücklich auf ein einzelnes Paket begrenzter Nutzerauftrag bleibt auf dieses Paket begrenzt.

```text
Setze den Völund-Plan innerhalb des beauftragten Umfangs selbstständig um.
Beginne mit Paket A01 aus docs/implementation-plan/03-fundament.md.
Bearbeite jeweils nur ein Paket, prüfe es und fahre nach dessen Abnahme
mit dem nächsten zulässigen Paket ohne erneute Aufforderung fort.

Lies zuerst:
- gegebenenfalls vorhandene AGENTS.md für die betroffenen Pfade;
- docs/implementation-plan/README.md, Abschnitte Ausführungsregeln und Reihenfolge;
- docs/implementation-plan/10-autonome-umsetzung.md;
- docs/implementation-plan/01-bestandsaufnahme.md, Befund F01 und durchgeführte Prüfungen;
- docs/architecture-and-code-contract.md, Anforderungen an Prüfungen/Quellgrenzen;
- tools/cmake/CheckSourceLimits.cmake;
- den betroffenen Aufruf in tests/CMakeLists.txt und CMakeLists.txt.

Ziel: Source-Limit-Prüfung darf mit ungültigem Quellpfad oder null geprüften
Dateien nicht grün werden. Das Quellpfadargument muss korrekt übergeben werden.
Prüfe Fixtures mit gültigem Pfad, Umlaut/Leerzeichen, literal Quotes,
nicht existierendem Verzeichnis und einer Quelle mit 601 Zeilen.

Der aktuelle echte Repositorycheck wird nach dieser Reparatur zunächst vier
Dateien über 600 Zeilen melden. Das ist die erwartete aufgedeckte Altlast.
Erhöhe das Limit nicht und schließe die Dateien nicht aus; die Aufteilung
ist Paket A03. Melde A01 als fachlich bestanden, aber das Gesamtgate G0
weiter rot, solange A03 fehlt.

Ändere im ersten Schritt nur A01 und seine nötigen Tests. Kein Python, kein Dependencyupgrade,
kein neuer CAD-Befehl, keine Oberflächenänderung. Erhalte Benutzeränderungen.
Prüfe den aktuellen Code statt unverändert von Stand 1b78229 auszugehen.
Führe die Paketprüfungen und relevanten vorhandenen Checks aus.

Liefere: Änderung, tatsächliche Testbefehle und Resultate, betroffene Dateien,
bekannte verbleibende Altlasten. Aktualisiere Paketstatus und Evidenz erst
nach erfolgreicher Paketabnahme. Fahre anschließend mit dem nächsten zulässigen
Paket fort. Übernimm Tests und optische Prüfungen selbst; hole den Nutzer nur
für einen belegten, anderweitig nicht lösbaren Rest gemäß Kapitel 10 hinzu.
```

## Vorlage für jedes weitere Paket

```text
Bearbeite als nächsten begrenzten Schritt Paket <ID> aus
docs/implementation-plan/<Kapitel>.md. Bei beauftragter Gesamtumsetzung
arbeite danach selbstständig weiter; bei einem ausdrücklichen Einzelauftrag
halte dessen Umfang ein. Kapitel 10 gilt für Ausführung und Prüfung.

1. Prüfe Arbeitsbaum und aktuellen Commit. Erhalte vorhandene Änderungen.
2. Lies Paket, dortige Einstiegspunkte und die einschlägigen Zielverträge aus
   docs/implementation-plan/02-zielvertraege.md. Lies zugehörige Szenarien aus
   08-abnahme.md und ggf. eine inzwischen abgeschlossene Entscheidungs-ADR.
3. Prüfe Voraussetzungen anhand packets.json UND deren realer Evidenz.
   Ein geplantes Paket oder ein nicht ausgeführter Plattformtest ist nicht done.
4. Nenne kurz den lokalen Änderungsplan und das beobachtbare Fertigkriterium.
5. Implementiere ausschließlich die Paketverantwortlichkeit. Bei mehr als fünf
   produktiven Quelldateien oder neuer Architekturfrage: zuerst das Paket in
   konkrete Unterpakete zerlegen bzw. die fehlende Entwurfsentscheidung benennen.
   Die Grenze nicht durch übergroße Sammeldateien umgehen.
6. Bei verhaltensänderndem Bugfix reproduzierenden Test ergänzen. Teste
   Fachinvarianten, Fehler/Cancel/Undo und relevante Persistenzpfade.
7. Führe passende bestehende Tests und inzwischen verpflichtende Architektur-
   checks aus. Bediene bei UI-Änderungen die echte Anwendung, erzeuge Bilder,
   prüfe sie selbst optisch und korrigiere Abweichungen. Fehlende Werkzeuge/Hosts
   als not-run melden, nicht als passed; verfügbare Alternativen zuerst prüfen.
8. Prüfe den Diff auf Nebenänderungen, falsche Erfolgsmeldungen, ungebundene UI,
   neue globale Zustände, unsichere Dateipfade und unvollständige Fehlerpfade.
9. Dokumentiere Ergebnis und Evidenz; aktualisiere den Paketstatus konsistent.
   Wähle bei Gesamtumsetzung das nächste Paket mit erfüllten Abhängigkeiten und
   Produktgates und arbeite weiter. Ist ein Paket extern blockiert, führe andere
   zulässige unabhängige Arbeit fort. Veröffentliche nichts außerhalb der im
   aktuellen Auftrag erteilten Autorisierung.

Verbote: Test abschalten, Grenzwerte aufblasen, Python einführen, Upstream
ungeprüft ändern, neue Library ohne vorgesehenen ADR-Prozess, unbekannte
Flächenreferenz geometrisch raten, fehlenden Handler durch No-op kaschieren.

Fertigmeldung: Problem/Verhalten vorher→nachher; geänderte Dateien; echte
Prüfbefehle/Ergebnisse einschließlich optischer Evidenz; verbleibende Risiken/Gates.
Bei laufender Gesamtumsetzung genügt eine kurze Fortschrittsmeldung mit Fortsetzung.
```

## Wie ein Paket gelesen und übergeben wird

Jeder Auftrag bekommt ein kurzes Paketdossier, nicht die gesamte Gesprächshistorie:

| Feld | Inhalt |
|---|---|
| Ausgangsrevision | Tatsächlicher HEAD; bei Abweichung vom Audit relevante Änderungen zuerst prüfen |
| Paket-ID | Genau eine Haupt- oder verbindlich definierte Unter-ID |
| Verträge | Nur einschlägige K-Abschnitte plus bindende Originalregel |
| Voraussetzungen | Fertige IDs mit Link auf Evidenz; Plattformgate gesondert |
| Einstiegspunkte | Aus dem Paket; nach Refactor anhand Symbol/Verantwortlichkeit aktualisieren |
| erwartetes Verhalten | Erfolg, Fehler, Cancel, Undo und Persistenz nach Paketumfang |
| Abnahme | Konkrete Szenario-IDs und CTest-/Buildbefehle |
| Grenzen | Keine Nebenfeatures; ausdrücklich noch nicht unterstützte Fälle |
| Ergebnis | Diff, Testnachweise, Status, tatsächliche offene Punkte |

Ein API-Entwurf enthält Felder, Besitz/Lebensdauer, Units, Statuswerte, Mutation/Transaktionsgrenze und zwei Beispielaufrufe einschließlich Fehler. „Robuste Referenzen implementieren“ ohne diese Informationen ist **kein ready Codingpaket**.

## Statusmodell und Nachweise

`planned → ready → in-progress → review → done`; zusätzlich `blocked-external` oder `needs-design`. Ein Paket ist nicht allein deshalb done, weil es kompiliert. Statusänderungen und Evidenz im Manifest konsistent halten. Für Unterpakete Eltern-ID, Reihenfolge und individuelle Abnahme ergänzen; Elternpaket erst done, wenn alle Kinder und die Integrationsabnahme fertig sind.

Empfohlener kurzer Evidenztext je abgeschlossenem Paket unter `docs/implementation-plan/evidence/<ID>.md`:

```text
Paket / Commit / Datum / Umgebung:
Erfüllter Vertrag und beobachtbares Verhalten:
Geänderte Dateien:
Prüfbefehle mit Exitcode und Ergebnis:
Geprüfte positive und negative Szenarien:
Migration / Plattformwirkung:
Nicht ausgeführte Checks und konkreter Grund:
Reviewbefund und verbleibende Gateblocker:
Optische Prüfung: Szenario, Bildpfade, tatsächlich betrachtet, Befund/Korrektur:
Restbedarf an Nutzerprüfung: konkrete technische Grenze oder keiner:
```

**Sonderfall ehrlicher Altlastentest:** A01 kann seine Abnahme erfüllen, obwohl der nun korrekte Gesamtcheck bis A03 rot bleibt. Das Ergebnis muss dann genau so dokumentiert werden. Kein späteres Featurepaket darf diesen bekannten Rotstand pauschal als Freifahrtschein für neue Verstöße verwenden.

## Prüfung durch Architektur-/Integrationsverantwortlichen

Besonders D04/D08/D11/R02/R08/R09 sowie neue G5-Fachverträge brauchen eine inhaltliche Gegenprüfung: eindeutige Datenautorität, deterministische Referenzen, Ressourcen-/Fehlergrenzen und Abwärtskompatibilität. Der Agent führt sie als gesonderten Prüfschritt anhand von Diff, Verträgen und Fehlerfixtures selbst durch. Das ist keine unabhängige Zweitmeinung und wird nicht als solche ausgegeben. Eine ausdrücklich verfügbare/autorisierte prüfende Instanz kann ergänzen; eine Nutzerabnahme oder das automatische Starten weiterer Agenten ist keine Standardvoraussetzung. Echte Entscheidungsgrenzen werden nach Kapitel 10 vorgelegt.

Prüffragen: Kann der Test trotz fehlender produktiver Verbindung bestehen? Bleibt ein Fehlzustand speicherbar? Wird eine mehrdeutige Referenz sichtbar? Entspricht Preview dem Commit? Erzeugt ein Drag genau einen Schritt? Übernimmt die neue Funktion dieselben Identitäten/Units/Transactions? Wenn die Antworten unklar sind, Paket zurück in `needs-design` statt eine neue lokale Heuristik einzubauen.

## Wiederaufnahme nach Unterbrechung

Aktuellen Arbeitsbaum, Paketstatus, letzten Evidenzbericht und relevante Symbole lesen. Bereits abgeschlossene Arbeiten nicht erneut implementieren. Nicht aus veralteten Zeilennummern oder alter Promptzusammenfassung auf aktuellen Zustand schließen. Der Plan wird anhand echter Änderungen weitergeführt; ein Chatwechsel ist kein Architektur-Neustart.
