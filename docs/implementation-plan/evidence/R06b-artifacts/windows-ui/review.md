# Sichtprüfung Windows – R06b

18.09.2026, visually-reviewed durch Bildlesewerkzeug.
Quelle: desktop.feature_status aus aktuellem Build, Quellstand gemäß
../source-hashes.json. Native Qt-Widgetaufnahme (MainWindow::grab), nach
isRenderingReady und Verarbeitung der UI-Events. Kein Offscreen-/Compositorbeleg.

- legacy-plane-migration.png, 1400×900: Skizze [error], Extrusion [blocked];
  Properties und Statuszeile zeigen MigrationNeedsRepair vollständig. Öffnen und
  Bearbeitungsversuch bleiben ohne implizite Reparatur. Leeres Gesamtmodell bei
  fehlerhaftem Open ist erwartet; Gitter sichtbar, kein schwarzer Renderer.
- feature-failed-blocked.png, 1200×800: Fehler und abhängige Blockierung getrennt;
  letzte gültige Gesamtgeometrie durch Statusbadge gekennzeichnet.
- feature-independent-current.png, 1200×800: unabhängiger Körper ohne Fehlerstatus
  auswählbar, Positions-/Größenfelder sichtbar. Dies ist die bestehende
  Fehlerisolationsregression, kein neuer Reparaturdialog.

Keine abgeschnittenen Diagnosen, Überlagerungen oder fehlenden Steuerelemente
im geprüften Umfang. R06c muss Kandidatenauswahl/Abbruch zusätzlich prüfen.
