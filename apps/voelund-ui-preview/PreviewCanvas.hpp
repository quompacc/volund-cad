#pragma once

#include <QWidget>

namespace voelund::preview {

class PreviewCanvas final : public QWidget {
    Q_OBJECT

public:
    enum class Mode { Solid, Sketch };

    explicit PreviewCanvas(QWidget* parent = nullptr);
    void setMode(Mode mode);
    [[nodiscard]] Mode mode() const noexcept { return mode_; }

signals:
    void selectionChanged(const QString& label);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    void paintBackground(QPainter& painter) const;
    void paintSolid(QPainter& painter) const;
    void paintSketch(QPainter& painter) const;
    void paintViewCube(QPainter& painter) const;
    void paintNavigation(QPainter& painter) const;

    Mode mode_{Mode::Solid};
    bool sideSelected_{false};
};

} // namespace voelund::preview
