#pragma once

#include <QColor>
#include <QIcon>
#include <QString>

#include <optional>

namespace voelund::desktop {

// The symbol of a sketch command ("sketch.tool.line", "sketch.parallel", …); nullopt for
// commands without one.
[[nodiscard]] std::optional<QIcon> sketchIcon(const QString& commandId, const QColor& ink,
                                              const QColor& accent);

} // namespace voelund::desktop
