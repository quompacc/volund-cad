#include "../SmokeSupport.hpp"
#include "PreviewStyle.hpp"
#include "PreviewWindow.hpp"
#include "SketchBackend.hpp"

#include <voelund/application/CadSession.hpp>

#include <QApplication>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QKeyEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QSurfaceFormat>
#include <QTimer>
#include <QToolButton>

#include <memory>

namespace {
bool verifySketchBackend(voelund::application::CadSession& session) {
    voelund::preview::SketchBackend backend(session);
    using SketchTool = voelund::preview::PreviewCanvas::SketchTool;
    using Intent = voelund::viewport::SketchElementIntent;
    if (!backend.begin())
        return false;
    backend.setTool(SketchTool::Line);
    const bool line =
        backend
            .confirmElement(Intent{-10.0, -8.0, 10.0, -8.0, std::nullopt, std::nullopt,
                                   std::nullopt, std::nullopt, std::nullopt, std::nullopt})
            .has_value();
    backend.setTool(SketchTool::Polyline);
    const bool polyline =
        backend
            .confirmElement(Intent{-10.0, -4.0, 10.0, -2.0, std::nullopt, std::nullopt,
                                   std::nullopt, std::nullopt, std::nullopt, std::nullopt})
            .has_value();
    backend.setTool(SketchTool::Rectangle);
    const bool rectangle =
        backend
            .confirmElement(Intent{-8.0, -5.0, 8.0, 5.0, std::nullopt, std::nullopt, std::nullopt,
                                   std::nullopt, std::nullopt, std::nullopt})
            .has_value();
    backend.setTool(SketchTool::Circle);
    const bool circle =
        backend
            .confirmElement(Intent{20.0, 10.0, 25.0, 10.0, std::nullopt, std::nullopt, std::nullopt,
                                   std::nullopt, std::nullopt, std::nullopt})
            .has_value();
    backend.setTool(SketchTool::Arc);
    const bool arc =
        backend
            .confirmElement(Intent{30.0, 10.0, 35.0, 10.0, std::pair{30.0, 15.0}, std::nullopt,
                                   std::nullopt, std::nullopt, std::nullopt, std::nullopt})
            .has_value();
    backend.setTool(SketchTool::ThreePointArc);
    const bool threePointArc =
        backend
            .confirmElement(Intent{40.0, 10.0, 45.0, 15.0, std::pair{50.0, 10.0}, std::nullopt,
                                   std::nullopt, std::nullopt, std::nullopt, std::nullopt})
            .has_value();
    backend.setTool(SketchTool::Ellipse);
    const bool ellipse =
        backend
            .confirmElement(Intent{60.0, 10.0, 66.0, 10.0, std::pair{60.0, 13.0}, std::nullopt,
                                   std::nullopt, std::nullopt, std::nullopt, std::nullopt})
            .has_value();
    backend.setTool(SketchTool::Slot);
    const bool slot =
        backend
            .confirmElement(Intent{70.0, 10.0, 80.0, 10.0, std::pair{70.0, 13.0}, std::nullopt,
                                   std::nullopt, std::nullopt, std::nullopt, std::nullopt})
            .has_value();
    backend.setTool(SketchTool::Polygon);
    const bool polygon =
        backend
            .confirmElement(Intent{90.0, 10.0, 95.0, 10.0, std::nullopt, std::nullopt, std::nullopt,
                                   std::nullopt, std::nullopt, std::nullopt})
            .has_value();
    if (!line || !polyline || !rectangle || !circle || !arc || !threePointArc || !ellipse ||
        !slot || !polygon || backend.sceneElements().empty() || !backend.finish()) {
        return false;
    }

    return true;
}
bool verifyInspectorScroll(voelund::preview::PreviewWindow& window) {
    auto* scroll = window.findChild<QScrollArea*>(QStringLiteral("inspectorFieldsScroll"));
    if (!scroll)
        return false;
    const auto fields = scroll->widget()->findChildren<QComboBox*>();
    if (fields.size() != 2)
        return false;
    for (auto* field : fields) {
        if (field->height() < field->sizeHint().height())
            return false;
        scroll->ensureWidgetVisible(field);
        QCoreApplication::processEvents();
        const QRect fieldRect(field->mapTo(scroll->viewport(), QPoint{}), field->size());
        if (!scroll->viewport()->rect().contains(fieldRect))
            return false;
    }
    const QString output = qEnvironmentVariable("VOELUND_SMOKE_ARTIFACT_DIR");
    if (!output.isEmpty() && scroll->verticalScrollBar()->maximum() > 0 &&
        !window.grab().save(output + "/preview-inspector-scrolled.png"))
        return false;
    scroll->verticalScrollBar()->setValue(0);
    return true;
}

void startPreviewSmoke(QApplication& application, voelund::preview::PreviewWindow& window) {
    auto* viewport = dynamic_cast<voelund::viewport::CadViewport*>(
        window.findChild<QWidget*>(QStringLiteral("previewCadViewport")));
    auto* extrude = window.findChild<QToolButton*>(QStringLiteral("previewExtrudeAction"));
    auto* distance =
        window.findChild<QDoubleSpinBox*>(QStringLiteral("previewExtrudeDistanceInput"));
    auto elapsed = std::make_shared<QElapsedTimer>();
    elapsed->start();
    auto* timer = new QTimer(&window);
    timer->setInterval(50);
    QObject::connect(timer, &QTimer::timeout, &application,
                     [&application, &window, viewport, extrude, distance, timer, elapsed] {
                         if (viewport == nullptr || extrude == nullptr || distance == nullptr) {
                             application.exit(2);
                         } else if (!voelund::apps::verifyDesktopSmoke(*viewport)) {
                             if (elapsed->elapsed() >= 10'000)
                                 application.exit(1);
                         } else {
                             timer->stop();
                             if (!voelund::apps::clickSmokeButton(*extrude) ||
                                 !voelund::apps::enterSmokeValue(*distance, QStringLiteral("31"))) {
                                 application.exit(3);
                                 return;
                             }
                             QCoreApplication::processEvents();
            if (!window.smokeTestPassed() || !extrude->property("active").toBool() ||
                distance->value() != 31.0 || distance->height() < distance->sizeHint().height() ||
                !verifyInspectorScroll(window)) {
                                 application.exit(3);
                                 return;
                             }
                             // The preview distance is not yet bound to CAD geometry (U11).
                             // Here we check the visible input path, not a completed extrusion.
                             QTimer::singleShot(150, &window, [&application, &window, viewport] {
                                 application.exit(
                                     voelund::apps::verifyDesktopSmoke(*viewport) &&
                                             voelund::apps::captureDesktopSmoke(window, *viewport)
                                         ? 0
                                         : 9);
                             });
                         }
                     });
    timer->start();
}
} // namespace

int main(int argc, char* argv[]) {
#if defined(VOELUND_OCCT_GLES)
    QSurfaceFormat surfaceFormat;
    surfaceFormat.setRenderableType(QSurfaceFormat::OpenGLES);
    surfaceFormat.setVersion(3, 0);
    surfaceFormat.setDepthBufferSize(24);
    surfaceFormat.setStencilBufferSize(8);
    QSurfaceFormat::setDefaultFormat(surfaceFormat);
#endif
    QApplication application(argc, argv);
    application.setApplicationDisplayName(QStringLiteral("Völund UI Preview"));
    application.setOrganizationName(QStringLiteral("Völund"));
    application.setStyleSheet(voelund::preview::applicationStyle());

    voelund::application::CadSession session;
    const bool smokeTest = application.arguments().contains(QStringLiteral("--smoke-test"));
    const bool uninitializedSmoke =
        application.arguments().contains(QStringLiteral("--smoke-test-uninitialized"));
    if (smokeTest || uninitializedSmoke)
        voelund::apps::enableSmokeLogging();
    if (uninitializedSmoke) {
        voelund::viewport::CadViewport viewport;
        QTimer::singleShot(0, &application, [&application, &viewport] {
            application.exit(voelund::apps::verifyDesktopSmoke(viewport) ? 0 : 10);
        });
        return application.exec();
    }
    if (smokeTest && !verifySketchBackend(session))
        return 2;

    voelund::preview::PreviewWindow window(session);
    window.show();

    if (smokeTest)
        startPreviewSmoke(application, window);

    return application.exec();
}
