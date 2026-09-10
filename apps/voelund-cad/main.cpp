#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>
#include <voelund/document/ModelDocument.hpp>
#include <voelund/viewport/CadViewport.hpp>

#include <QApplication>
#include <QAction>
#include <QCoreApplication>
#include <QMouseEvent>
#include <QTimer>

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
    voelund::application::CadSession session;
    const bool isSmokeTest = QCoreApplication::arguments().contains("--smoke-test");
    if (isSmokeTest) {
        static_cast<void>(session.selectFeature(
            voelund::document::ModelDocument::initialCylinderId()));
        voelund::document::CylinderParameters parameters =
            session.selectedCylinderParameters();
        parameters.resultMode = voelund::document::CylinderResultMode::separate;
        static_cast<void>(session.setSelectedCylinderParameters(parameters));
    }
    voelund::desktop::MainWindow mainWindow(session);
    mainWindow.show();

    if (isSmokeTest) {
        QTimer::singleShot(250, &application, [&application, &mainWindow]() {
            auto* viewport =
                dynamic_cast<voelund::viewport::CadViewport*>(mainWindow.centralWidget());
            if (viewport == nullptr || !viewport->isRenderingReady()) {
                QCoreApplication::exit(1);
                return;
            }

            QAction* transformAction =
                mainWindow.findChild<QAction*>(QStringLiteral("transformAction"));
            if (transformAction == nullptr) {
                QCoreApplication::exit(1);
                return;
            }
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
    }
    return application.exec();
}
