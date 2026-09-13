#pragma once
#include <QLayout>
#include <QScrollArea>

namespace voelund::preview {
// Keep the natural panel height when space allows it. On short/high-DPI
// windows scroll the fields instead of collapsing their text; buttons stay outside.
class InspectorScrollArea final : public QScrollArea {
  public:
    explicit InspectorScrollArea(QWidget* content) {
        setObjectName(QStringLiteral("inspectorFieldsScroll"));
        content->setObjectName(QStringLiteral("inspectorFieldsContent"));
        setFrameShape(QFrame::NoFrame);
        setWidgetResizable(true);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        content->layout()->setSizeConstraint(QLayout::SetMinimumSize);
        setWidget(content);
    }
    QSize sizeHint() const override { return widget()->sizeHint(); }
    QSize minimumSizeHint() const override { return {80, 80}; }
};
} // namespace voelund::preview
