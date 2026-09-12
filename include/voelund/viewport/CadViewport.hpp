#pragma once

#include <voelund/kernel/OcctCompatibility.hpp>

#if defined(VOELUND_VIEWPORT_QT_OPENGL)
#include <QOpenGLWidget>
#else
#include <QWidget>
#endif

#include <TopoDS_Shape.hxx>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>

class QMouseEvent;
class QPaintEngine;
class QPaintEvent;
class QResizeEvent;
class QShowEvent;
class QWheelEvent;

namespace voelund::viewport {

struct SceneBody final {
    std::uint64_t id;
    TopoDS_Shape shape;
    bool transformable;
};

struct SceneSketchElement final {
    std::uint64_t id;
    TopoDS_Shape shape;
};

struct PlanarFaceSelection final {
    std::uint64_t bodyId;
    double originX;
    double originY;
    double originZ;
    double normalX;
    double normalY;
    double normalZ;
    double xDirectionX;
    double xDirectionY;
    double xDirectionZ;
};

struct SketchRectangleResult final {
    PlanarFaceSelection support;
    double firstX;
    double firstY;
    double secondX;
    double secondY;
    double distance;
};

enum class SketchElementTool { line, rectangle, circle };

struct SketchElementIntent final {
    double firstX;
    double firstY;
    double secondX;
    double secondY;
};

#if defined(VOELUND_VIEWPORT_QT_OPENGL)
using CadViewportBase = QOpenGLWidget;
#else
using CadViewportBase = QWidget;
#endif

class CadViewport final : public CadViewportBase {
public:
    explicit CadViewport(QWidget* parent = nullptr);
    ~CadViewport() override;

    void setShape(const TopoDS_Shape& shape);
    void setBodies(std::span<const SceneBody> bodies);
    void setSketchElements(
        std::span<const SceneSketchElement> elements,
        std::optional<std::uint64_t> selectedId = std::nullopt);
    void setSketchElementSelectionHandler(
        std::function<void(std::uint64_t)> handler);
    void setSelectedSketchElement(std::optional<std::uint64_t> elementId);
    void setBodySelectionHandler(std::function<void(std::uint64_t)> handler);
    void setPlanarFaceSelectionHandler(
        std::function<void(const PlanarFaceSelection&)> handler);
    void setSketchRectangleHandler(
        std::function<void(const SketchRectangleResult&)> handler);
    void setSketchExtrusionPreviewHandler(std::function<void(double)> handler);
    void setSketchEditorPreviewHandler(
        std::function<void(const std::optional<SketchElementIntent>&)> handler);
    void setSketchEditorElementHandler(
        std::function<void(const SketchElementIntent&)> handler);
    void setSketchEditorTranslationHandler(
        std::function<void(double, double)> handler);
    void beginRectangleSketch(const PlanarFaceSelection& support);
    void beginSketchEditor(const PlanarFaceSelection& support, SketchElementTool tool);
    void setSketchEditorTool(SketchElementTool tool);
    void updateSketchExtrusionPreview(double distance);
    void cancelSketch();
    void setBodyMoveHandler(
        std::function<void(std::uint64_t, double, double, double)> handler);
    void setBodyMovePreviewHandler(
        std::function<void(std::uint64_t, double, double, double)> handler);
    void setBodyRotationHandler(
        std::function<void(std::uint64_t, int, double)> handler);
    void setBodyRotationPreviewHandler(
        std::function<void(std::uint64_t, int, double)> handler);
    void setTransformToolEnabled(bool enabled);
    void setTransformTarget(std::optional<std::uint64_t> bodyId);
    void setTransformSnapping(bool enabled, double linearStep, double angularStep);
    [[nodiscard]] bool isRenderingReady() const noexcept;
    [[nodiscard]] std::size_t displayedSketchElementCount() const noexcept;

protected:
#if defined(VOELUND_VIEWPORT_QT_OPENGL)
    void initializeGL() override;
    void paintGL() override;
#else
    [[nodiscard]] QPaintEngine* paintEngine() const override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
#endif
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace voelund::viewport
