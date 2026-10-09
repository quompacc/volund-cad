#include "SelectionBand.hpp"

#include <QPainter>
#include <QPen>

namespace voelund::viewport {

SelectionBand::SelectionBand(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_NoSystemBackground);
    hide();
}

void SelectionBand::setCrossing(bool crossing) {
    if (crossing_ == crossing)
        return;
    crossing_ = crossing;
    update();
}

void SelectionBand::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    const QColor accent = crossing_ ? QColor(60, 170, 90) : QColor(40, 120, 220);
    QColor fill = accent;
    fill.setAlpha(40);
    painter.fillRect(rect(), fill);
    QPen pen(accent, 1.5, crossing_ ? Qt::DashLine : Qt::SolidLine);
    painter.setPen(pen);
    painter.drawRect(rect().adjusted(0, 0, -1, -1));
}

} // namespace voelund::viewport
