#pragma once

#include <voelund/viewport/CadViewport.hpp>

#include <QMainWindow>

#include <memory>
#include <optional>

class QLabel;
class QPushButton;
class QStackedWidget;
class QToolButton;
class QTreeWidget;
class QWidget;

namespace voelund::preview {

class PreviewCanvas;
class SketchBackend;
class ViewCube;

}

namespace voelund::application { class CadSession; }

namespace voelund::preview {

class PreviewWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit PreviewWindow(application::CadSession& session, QWidget* parent = nullptr);
    ~PreviewWindow() override;
    [[nodiscard]] bool smokeTestPassed() const;

private:
    QWidget* createTopBar();
    QWidget* createWorkspaceTabs();
    QWidget* createSolidRibbon();
    QWidget* createSketchRibbon();
    QWidget* createBrowser();
    QWidget* createSolidInspector();
    QWidget* createSketchInspector();
    QWidget* createTimeline();
    QWidget* createStatusBar();
    QWidget* createRibbonGroup(const QString& title, const QList<QWidget*>& commands);
    QWidget* createCommand(const QString& text, int icon, const QString& tooltip = {});
    QWidget* createField(const QString& label, QWidget* editor);
    QWidget* createCard(const QString& title, const QList<QWidget*>& rows);
    void enterSketch();
    void finishSketch();
    void activateExtrude();
    void closeActiveTool();
    void activateSketchTool(int tool, QToolButton* button);
    void setWorkspaceTab(const QString& activeName);
    void refreshBrowser();
    void refreshScene();
    void refreshSketchScene();
    void refreshStyle(QWidget* widget);

    PreviewCanvas* canvas_{nullptr};
    viewport::CadViewport* cadViewport_{nullptr};
    QStackedWidget* canvasStack_{nullptr};
    ViewCube* viewCube_{nullptr};
    std::optional<viewport::PlanarFaceSelection> selectedPlanarFace_;
    application::CadSession& session_;
    std::unique_ptr<SketchBackend> sketchBackend_;
    QStackedWidget* ribbonStack_{nullptr};
    QStackedWidget* inspectorStack_{nullptr};
    QTreeWidget* browserTree_{nullptr};
    QLabel* statusText_{nullptr};
    QLabel* selectionText_{nullptr};
    QLabel* sketchStateLabel_{nullptr};
    QPushButton* sketchWorkspaceTab_{nullptr};
    QList<QPushButton*> workspaceTabs_;
    QList<QToolButton*> sketchToolButtons_;
    QToolButton* extrudeButton_{nullptr};
};

} // namespace voelund::preview
