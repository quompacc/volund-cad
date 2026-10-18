#pragma once

#include <QString>

class QMenu;
class QWidget;

namespace voelund::desktop {

// L03 (user decision 2026-10-18): "Über Völund…" as the last entry of the main menu. The
// dialog shows version, revision, document schema, library versions, platform and the
// settings and recovery locations, and copies them as one diagnostics text.
void addAboutAction(QMenu& menu, QWidget& parent);

// The diagnostics text of the dialog.
[[nodiscard]] QString diagnosticsText();

} // namespace voelund::desktop
