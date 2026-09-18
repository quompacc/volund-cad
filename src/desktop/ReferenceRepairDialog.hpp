#pragma once
#include <QDialog>
#include <functional>
#include <voelund/application/CadSession.hpp>
class QComboBox;
class QListWidget;
class QLabel;
class QPushButton;
class QCheckBox;
class QGraphicsView;
namespace voelund::desktop {
class ReferenceRepairDialog final : public QDialog {
  public:
    ReferenceRepairDialog(application::CadSession& session, document::FeatureId sketch,
                          std::function<void()> changed, QWidget* parent);

  private:
    void reload();
    void populate();
    void selectionChanged();
    void apply();
    void preview(const application::SketchSupportProjection& projection,
                 const application::ProjectedSketchPoint* point);
    application::CadSession& session_;
    document::FeatureId sketch_;
    std::function<void()> changed_;
    std::optional<application::SketchReferenceInspection> inspection_;
    std::vector<std::vector<document::SketchConstraintId>> slots_;
    std::vector<application::ProjectedSketchPoint> points_;
    QComboBox* target_;
    QListWidget* candidates_;
    QLabel* original_;
    QLabel* message_;
    QLabel* previewInfo_;
    QGraphicsView* preview_;
    QPushButton* apply_;
    QCheckBox* keepErrors_;
};
} // namespace voelund::desktop
