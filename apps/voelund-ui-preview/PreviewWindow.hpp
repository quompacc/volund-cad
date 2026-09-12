#pragma once

#include <QMainWindow>

class QLabel;
class QPushButton;
class QStackedWidget;
class QTreeWidget;
class QWidget;

namespace voelund::preview {

class PreviewCanvas;

class PreviewWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit PreviewWindow(QWidget* parent = nullptr);
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
    void setWorkspaceTab(const QString& activeName);
    void refreshStyle(QWidget* widget);

    PreviewCanvas* canvas_{nullptr};
    QStackedWidget* ribbonStack_{nullptr};
    QStackedWidget* inspectorStack_{nullptr};
    QTreeWidget* browserTree_{nullptr};
    QLabel* statusText_{nullptr};
    QLabel* selectionText_{nullptr};
    QList<QPushButton*> workspaceTabs_;
};

} // namespace voelund::preview
