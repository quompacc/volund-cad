#pragma once

#include <QColor>
#include <QIcon>

namespace voelund::preview {

enum class Icon {
    Logo,
    Sketch,
    Extrude,
    Revolve,
    Hole,
    Fillet,
    Chamfer,
    Shell,
    Joint,
    Line,
    Rectangle,
    Circle,
    Arc,
    Constraint,
    Dimension,
    Trim,
    Offset,
    Finish,
    Search,
    Help,
    Settings,
    Eye,
    Chevron,
    Select,
};

QIcon makeIcon(Icon icon, const QColor& color = QColor(QStringLiteral("#C7D0DB")));

} // namespace voelund::preview
