#pragma once

#include <QString>

#include <voelund/document/BodyId.hpp>

#include <exception>
#include <functional>
#include <memory>
#include <vector>

class QAction;
class QMainWindow;
class QMenu;
namespace voelund::application {
class CadSession;
struct StepImportResult;
} // namespace voelund::application

namespace voelund::desktop {

class RecentFiles;

class DocumentActions final {
  public:
    DocumentActions(QMainWindow& window, application::CadSession& session,
                    std::function<void()> refreshHandler);
    ~DocumentActions();
    DocumentActions(const DocumentActions&) = delete;
    DocumentActions& operator=(const DocumentActions&) = delete;
    [[nodiscard]] bool confirmClose();
    // U07: opens a file as File > Open does, without the dialog (a file argument). Unsaved
    // changes are asked for first; a failure keeps the active document.
    void openFile(const QString& fileName);
    // L05: replaces the document by restored work, after the unsaved-changes question, as Open
    // does. The result has no file; a failure keeps the active document.
    [[nodiscard]] bool restore(const std::function<void()>& apply);
    [[nodiscard]] QMenu& menu() const;
    void syncWindowTitle();
    // Called whenever the title changes, so the shell header can follow (U03).
    void setShellChangedHandler(std::function<void()> handler);
    // M18: the bodies chosen for an export; none exports every visible body.
    void setExportSelection(std::function<std::vector<document::BodyId>()> selection);
    // M18: import and export wait while a tool is open; export needs a current body.
    void syncExchange(bool toolOpen);

  private:
    void newDocument();
    void duplicateDocument();
    [[nodiscard]] bool confirmDuplicate();
    void open();
    void load(const QString& fileName);
    [[nodiscard]] bool confirmSaveChanges();
    [[nodiscard]] bool save();
    [[nodiscard]] bool saveAs();
    [[nodiscard]] bool saveTo(const QString& fileName);
    void showError(const QString& operation, const std::exception& error);
    // M18 (user decision 2026-10-16): STEP import into the open document and export.
    void importStep();
    void exportStep();
    void reportImport(const application::StepImportResult& result);

    QMainWindow& window_;
    application::CadSession& session_;
    std::function<void()> refreshHandler_;
    QMenu* fileMenu_{nullptr};
    QAction* importAction_{nullptr};
    QAction* exportAction_{nullptr};
    std::function<void()> shellChanged_;
    std::function<std::vector<document::BodyId>()> exportSelection_;
    std::unique_ptr<RecentFiles> recent_;
    QString remembered_; // the file last put on the recent list
};

} // namespace voelund::desktop
