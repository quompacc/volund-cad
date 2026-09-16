# D11 – Identitätsabnahme und Übergang zu D12

Status: **D11 abgeschlossen**, 16.09.2026. Geprüfter Quellstand:
`b71a081740a70161e106bef9942f44d195ccf27b` (Implementierung bis D11n).
Dieser Dokumentationsblock ändert keine Produktquellen und kein Dateiformat.
Er ersetzt die bisherigen vorläufigen D11-Offenvermerke, nicht deren historischen
Prüfergebnisse. D12, D13, A-BODY-03 und Meridian L07 bleiben offen.

## Abnahmematrix

Die Kriterien stammen unverändert aus [D11](../03-fundament.md#d11--dokument--körper--und-featureidentität-trennen).

| Kriterium / Auftrag | Nachweis am aktuellen Stand | Ergebnis und Reichweite |
| --- | --- | --- |
| Getrennte DocumentId, BodyId und FeatureId | [D11a](D11a.md), [D11e](D11e.md), `document.identity_generator`, `document.identity_lifecycle` | Erfüllt: typisierte Namensräume, nicht aus Name oder Pfad abgeleitet. |
| Körper und Shape-Erzeuger separat | [D11c](D11c.md), `features.body_identity`, `modifiersAndModeChanges()` | Erfüllt: Folgeoperation wechselt den Erzeuger, erhält den Zielkörper; Separate besitzt eigene Herkunft. |
| Umbenennen erhält Körperidentität | `BodyIdentityEvaluationTests::namesAndOrder`, [D11l](D11l.md) / `document.native_schema` | Erfüllt für Legacy- und native Datenübergänge; kein Nachweis eines neuen Rename-UI-Befehls. |
| Save-As und weiteres Feature erhalten Körperidentität | `BodyIdentityEvaluationTests::applicationSaveAs` und `modifiersAndModeChanges`, [D11m](D11m.md) | Erfüllt im Produktpfad einschließlich Legacy-Open für BodyIds; DocumentId bleibt bei Save-As in der Sitzung erhalten. |
| Zwei Körper bleiben nach Reorder getrennt | `namesAndOrder`, `document.native_schema` | Erfüllt mit mehreren unabhängigen Körpern und zulässiger Reihenfolge; Timeline-Reorder und beliebige historische Geometrieinputs sind nicht abgenommen. |
| Reproduzierbare Legacy-Zuordnung | `document.legacy_body_identity`, [D11a](D11a.md) | Erfüllt für VOLUND 1–16, Namen, umgeordnete Datenpaare und lange Ketten. |
| Ungültige IDs werden abgewiesen | `document.legacy_body_identity`, native ID-/Schema-Tests, [D11d](D11d.md) bis [D11l](D11l.md) | Erfüllt für ungültige/reservierte IDs, Duplikate und beschädigte Referenzen gemäß jeweiligem Feldvertrag. Profil-IDs sind keine Vergabezähler. |
| Vergabe, Nichtwiederverwendung, Überlauf vor Mutation | `document.feature_id_lifetime`, [D11b](D11b.md), [D11m](D11m.md) | Erfüllt für veröffentlichte IDs im lebenden und ausdrücklich gespeicherten Vergabestand; abgebrochene Kandidaten verbrauchen keine IDs. |
| Kopie, Revision, explizites Duplikat | `document.identity_lifecycle`, `application.document_identity`, `desktop.document_actions` | Erfüllt: Wertkopie/Revision behalten DocumentId; Duplikat erhält neue ID, denselben Inhalt, keinen Pfad und keine Historie. Save-/Cancel-/Fehlerpfade sind geprüft. |

Testquellen: [Körperauswertung](../../../tests/integration/BodyIdentityEvaluationTests.cpp),
[natives Gesamtschema](../../../tests/integration/NativeSchemaTests.cpp),
[Anwendungsidentität](../../../tests/integration/CadSessionIdentityTests.cpp),
[Desktop-Aktionen](../../../tests/desktop/DocumentActionsTests.cpp).

## Abgrenzung ohne Abhängigkeitskreis

D11 definiert und prüft Identität; D12 übernimmt sie in die private OCAF-Autorität;
D13 bindet diese an den Produktstore an. Die Abhängigkeit bleibt **D11 → D12 → D13**.
D11 auf den produktiven Store warten zu lassen würde diese Reihenfolge blockieren.
Die folgenden Arbeiten werden durch den D11-Abschluss ausdrücklich nicht als
fertig erklärt:

| Offene Arbeit | Zuständigkeit / Abnahme |
| --- | --- |
| OCAF als einzige Modellautorität, Transaktionen, Undo/Redo, unveränderliche DTOs | D12; erster Schritt [D12a](../D12a-plan.md). |
| DocumentId über produktives Save/Open erhalten, nativer Dispatch, AtomicFileStore, Save-Revision | D13; plattformübergreifender Produktdateirundlauf und A-SAVE-01/02. |
| Ressourcenlimits vor OCCT-Open und Regeln für unbekannte Attribute | D13; die bisherigen Reader-Limits greifen erst nach Container-Allokation. |
| Legacy-Import → neues Save/Open/Edit/Undo vollständig abnehmen | D14. |
| Fehler eines Körpers von unabhängigen Zweigen isolieren, historische Inputs und DAG | R01. |
| Direkter Mehrkörperworkflow ohne Baugruppe, Körpernamen/Sichtbarkeit/Placement und explizites NewBody | S03/M01–M04, A-BODY-03; Identitäts-/Codec-Tests ersetzen keine Bedienabnahme. |
| Flächenidentität, Split/Merge, Assembly-Instanzen und Vault | R02–R06 bzw. spätere zugehörige Pakete. |

**VOLUND 16 bleibt Produktwriter und speichert keine DocumentId.** Erfolgreiches
Legacy-Open erzeugt daher auch bei derselben Datei eine neue DocumentId; BodyIds
werden deterministisch rekonstruiert. Die derzeitige BodyId-Zuordnung folgt noch
der Feature-Herkunft. Freie, von dieser Herkunft unabhängige Körperzuordnungen
und eigene Körpermetadaten sind keine bereits implementierte Fähigkeit.
Schließen ohne Speichern besitzt weiterhin keine versteckte Identitätssicherung.

## Prüfgrundlage

Die letzte vollständige Quellprüfung ist [D11n](D11n.md): **66/66 Windows**
(99,41 s), **62/62 FreeBSD/Sway** (35,31 s), 294 identische Quellhashes.
Der gemischte native Dateiaustausch ist zusätzlich in [D11l](D11l.md) belegt.
Für diesen reinen Dokumentationsblock wurden die 294 lokalen Quellhashes erneut
gegen das D11n-Manifest verglichen: unverändert. Plan-JSON (110 Pakete), lokale
Nachweislinks und Diff-Prüfung bestanden.
Die genannten Suite-Ergebnisse sind übernommene Nachweise, keine neuen Testläufe.
