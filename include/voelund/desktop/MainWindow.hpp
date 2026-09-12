#pragma once

#include <QMainWindow>

#include <cstdint>
#include <memory>

class QCloseEvent;

namespace voelund::application {
class CadSession;
}

namespace voelund::desktop {

class MainWindow final : public QMainWindow {
public:
    explicit MainWindow(application::CadSession& session, QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void createViewport();
    void createModelTree();
    void createPropertiesPanel();
    void createToolPanel();
    void createHistoryToolbar();
    void createWorkspaceToolbar();
    void createConnectionToolbar();
    void createTransformSettingsToolbar();
    void connectInteractions();
    void syncModelTree();
    void syncProperties();
    void syncTreeStatus();
    void syncSketchPresentation();
    void syncTransformTool();
    void refreshScene();
    void applyBoxParameters();
    void applyCylinderParameters();
    void applyCylinderResultMode();
    void applyExtrusionParameters();
    void previewBodyMove(std::uint64_t id, double deltaX, double deltaY, double deltaZ);
    void previewBodyRotation(std::uint64_t id, int axisIndex, double angleDegrees);
    void selectFeature(std::uint64_t rawId);

    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace voelund::desktop
