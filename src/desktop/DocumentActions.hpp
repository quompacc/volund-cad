#pragma once

#include <QString>

#include <exception>
#include <functional>

class QMainWindow;
class QMenu;
namespace voelund::application {
class CadSession;
}

namespace voelund::desktop {

class DocumentActions final {
  public:
    DocumentActions(QMainWindow& window, application::CadSession& session,
                    std::function<void()> refreshHandler);
    [[nodiscard]] bool confirmClose();
    [[nodiscard]] QMenu& menu() const;
    void syncWindowTitle();
    // Called whenever the title changes, so the shell header can follow (U03).
    void setShellChangedHandler(std::function<void()> handler);

  private:
    void newDocument();
    void duplicateDocument();
    [[nodiscard]] bool confirmDuplicate();
    void open();
    [[nodiscard]] bool confirmSaveChanges();
    [[nodiscard]] bool save();
    [[nodiscard]] bool saveAs();
    [[nodiscard]] bool saveTo(const QString& fileName);
    void showError(const QString& operation, const std::exception& error);

    QMainWindow& window_;
    application::CadSession& session_;
    std::function<void()> refreshHandler_;
    QMenu* fileMenu_{nullptr};
    std::function<void()> shellChanged_;
};

} // namespace voelund::desktop
