#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>
#include <voelund/document/ModelDocument.hpp>
#include <voelund/viewport/CadViewport.hpp>

#include <QApplication>
#include <QAction>
#include <QCoreApplication>
#include <QComboBox>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QSurfaceFormat>
#include <QTimer>

int main(int argc, char* argv[])
{
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
    if (isSmokeTest
        && qEnvironmentVariable("QT_QPA_PLATFORM") == QStringLiteral("wayland")
        && !QGuiApplication::platformName().startsWith(QStringLiteral("wayland"))) {
        return 2;
    }
    if (isSmokeTest) {
        static_cast<void>(session.selectFeature(
            voelund::document::ModelDocument::initialCylinderId()));
        voelund::document::CylinderParameters parameters =
            session.selectedCylinderParameters();
        parameters.resultMode = voelund::document::CylinderResultMode::separate;
        static_cast<void>(session.setSelectedCylinderParameters(parameters));
        static_cast<void>(session.selectFeature(
            voelund::document::ModelDocument::initialBoxId()));
    }
    voelund::desktop::MainWindow mainWindow(session);
    mainWindow.show();

    QTimer smokeTimer;
    QElapsedTimer smokeElapsed;
    if (isSmokeTest) {
        smokeElapsed.start();
        smokeTimer.setInterval(50);
        QObject::connect(&smokeTimer, &QTimer::timeout, &application,
                         [&mainWindow, &smokeTimer, &smokeElapsed]() {
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
            if (transformAction == nullptr || connectionModeInput == nullptr
                || !connectionModeInput->isVisible()
                || !connectionModeInput->isEnabled()) {
                QCoreApplication::exit(1);
                return;
            }
            smokeTimer.stop();
            transformAction->trigger();
            QCoreApplication::processEvents();

            // A click in empty space must be harmless even though OCCT has no detected owner.
            const QPointF localPosition(1.0, 1.0);
            const QPointF globalPosition(viewport->mapToGlobal(localPosition.toPoint()));
            QMouseEvent emptyClick(
                QEvent::MouseButtonRelease,
                localPosition,
                globalPosition,
                Qt::LeftButton,
                Qt::NoButton,
                Qt::NoModifier);
            QCoreApplication::sendEvent(viewport, &emptyClick);
            QCoreApplication::exit(0);
        });
        smokeTimer.start();
    }
    return application.exec();
}
