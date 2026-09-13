#pragma once

#include <QString>

#include <exception>
#include <functional>

class QMainWindow;
namespace voelund::application {
class CadSession;
}

namespace voelund::desktop {

class DocumentActions final {
  public:
    DocumentActions(QMainWindow& window, application::CadSession& session,
                    std::function<void()> refreshHandler);
    [[nodiscard]] bool confirmClose();
    void syncWindowTitle();

  private:
    void newDocument();
    void open();
    [[nodiscard]] bool confirmSaveChanges();
    [[nodiscard]] bool save();
    [[nodiscard]] bool saveAs();
    [[nodiscard]] bool saveTo(const QString& fileName);
    void showError(const QString& operation, const std::exception& error);

    QMainWindow& window_;
    application::CadSession& session_;
    std::function<void()> refreshHandler_;
};

} // namespace voelund::desktop
