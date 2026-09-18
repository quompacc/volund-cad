#pragma once
#include <QDialog>
#include <functional>
#include <voelund/application/CadSession.hpp>
class QTableWidget;
class QLabel;
class QCheckBox;
namespace voelund::desktop {
class DocumentRepairDialog final : public QDialog {
  public:
    DocumentRepairDialog(application::CadSession& session, std::function<void()> changed,
                         QWidget* parent);

  private:
    void reload();
    void propose();
    void apply();
    application::CadSession& session_;
    std::function<void()> changed_;
    std::vector<application::SketchSupportRepairSuggestion> suggestions_;
    QTableWidget* table_;
    QLabel* message_;
    QCheckBox* keep_;
};
} // namespace voelund::desktop
