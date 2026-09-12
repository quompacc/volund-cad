#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>
#include <voelund/document/ModelDocument.hpp>
#include <voelund/features/SketchEvaluation.hpp>
#include <voelund/viewport/CadViewport.hpp>

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QSurfaceFormat>
#include <QTimer>

#include <vector>

int main(int argc, char* argv[]) {
#if defined(VOELUND_OCCT_GLES)
    QSurfaceFormat surfaceFormat;
    surfaceFormat.setRenderableType(QSurfaceFormat::OpenGLES);
    surfaceFormat.setVersion(3, 0);
    surfaceFormat.setDepthBufferSize(24);
    surfaceFormat.setStencilBufferSize(8);
    QSurfaceFormat::setDefaultFormat(surfaceFormat);
#endif
    QCoreApplication::setApplicationName(QStringLiteral("Völund CAD"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1.0"));
    QCoreApplication::setOrganizationName(QStringLiteral("QuompaCC"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("quompacc.github.io"));
    QGuiApplication::setDesktopFileName(QStringLiteral("io.github.quompacc.Voelund"));
    QApplication application(argc, argv);
    voelund::application::CadSession session;
    const bool isSmokeTest = QCoreApplication::arguments().contains("--smoke-test");
    if (isSmokeTest && qEnvironmentVariable("QT_QPA_PLATFORM") == QStringLiteral("wayland") &&
        !QGuiApplication::platformName().startsWith(QStringLiteral("wayland"))) {
        return 2;
    }
    if (isSmokeTest) {
        static_cast<void>(
            session.selectFeature(voelund::document::ModelDocument::initialCylinderId()));
        voelund::document::CylinderParameters parameters = session.selectedCylinderParameters();
        parameters.resultMode = voelund::document::CylinderResultMode::separate;
        static_cast<void>(session.setSelectedCylinderParameters(parameters));
        static_cast<void>(session.selectFeature(voelund::document::ModelDocument::initialBoxId()));
    }
    std::vector<voelund::viewport::SceneSketchElement> smokeSketchElements;
    std::vector<voelund::viewport::SceneSketchConstraint> smokeSketchConstraints;
    if (isSmokeTest) {
        const voelund::document::SketchParameters sketch{
            {0.0, 0.0, 25.0, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0},
            {{{voelund::document::SketchElementId{1},
               voelund::document::SketchRectangle{0.0, 0.0, 5.0, 4.0}},
              {voelund::document::SketchElementId{2},
               voelund::document::SketchCircle{{8.0, 2.0}, 1.5}},
              {voelund::document::SketchElementId{3},
               voelund::document::SketchLine{{0.0, 7.0}, {5.0, 7.0}}}},
             4}};
        for (const auto& element : voelund::features::evaluateSketchElements(sketch)) {
            smokeSketchElements.push_back({element.id.value(), element.shape});
        }
        smokeSketchConstraints.push_back({1, "L 5.000 mm", 2.5, 0.0, 25.02, false, true});
        smokeSketchConstraints.push_back({2, "[H]", 2.5, 7.0, 25.02, false, false});
    }
    voelund::desktop::MainWindow mainWindow(session);
    mainWindow.show();

    QTimer smokeTimer;
    QElapsedTimer smokeElapsed;
    if (isSmokeTest) {
        smokeElapsed.start();
        smokeTimer.setInterval(50);
        QObject::connect(
            &smokeTimer, &QTimer::timeout, &application,
            [&mainWindow, &smokeTimer, &smokeElapsed, &smokeSketchElements,
             &smokeSketchConstraints]() {
                auto* viewport =
                    dynamic_cast<voelund::viewport::CadViewport*>(mainWindow.centralWidget());
                if (viewport == nullptr) {
                    QCoreApplication::exit(1);
                    return;
                }
                if (!viewport->isRenderingReady()) {
                    if (smokeElapsed.elapsed() >= 10'000) {
                        QCoreApplication::exit(1);
                    }
                    return;
                }

                QAction* transformAction =
                    mainWindow.findChild<QAction*>(QStringLiteral("transformAction"));
                QComboBox* connectionModeInput =
                    mainWindow.findChild<QComboBox*>(QStringLiteral("connectionModeInput"));
                if (transformAction == nullptr || connectionModeInput == nullptr ||
                    !connectionModeInput->isVisible() || !connectionModeInput->isEnabled()) {
                    QCoreApplication::exit(1);
                    return;
                }
                smokeTimer.stop();
                transformAction->trigger();
                QCoreApplication::processEvents();
                viewport->setSketchElements(smokeSketchElements, 2);
                viewport->setSketchConstraints(smokeSketchConstraints);
                QCoreApplication::processEvents();
                if (viewport->displayedSketchElementCount() != smokeSketchElements.size() ||
                    viewport->displayedSketchConstraintCount() != smokeSketchConstraints.size()) {
                    QCoreApplication::exit(1);
                    return;
                }

                // A click in empty space must be harmless even though OCCT has no detected owner.
                const QPointF localPosition(1.0, 1.0);
                const QPointF globalPosition(viewport->mapToGlobal(localPosition.toPoint()));
                QMouseEvent emptyClick(QEvent::MouseButtonRelease, localPosition, globalPosition,
                                       Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
                QCoreApplication::sendEvent(viewport, &emptyClick);
                QCoreApplication::exit(0);
            });
        smokeTimer.start();
    }
    return application.exec();
}
