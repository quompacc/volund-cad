#include "../SmokeSupport.hpp"

#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>
#include <voelund/document/ModelDocument.hpp>
#include <voelund/features/SketchEvaluation.hpp>
#include <voelund/viewport/CadViewport.hpp>

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QCoreApplication>
#include <QDebug>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QSurfaceFormat>
#include <QTimer>
#include <QWheelEvent>

#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QStandardPaths>
#include <QKeyEvent>
#include <cmath>

#include <memory>

namespace {
using voelund::application::CadSession;
using voelund::desktop::MainWindow;
using voelund::viewport::CadViewport;
bool verifyCadControls(MainWindow& mainWindow) {
    QAction* transformAction = mainWindow.findChild<QAction*>(QStringLiteral("transformAction"));
    QAction* addBoxAction = mainWindow.findChild<QAction*>(QStringLiteral("addBoxAction"));
    QAction* addCylinderAction =
        mainWindow.findChild<QAction*>(QStringLiteral("addCylinderAction"));
    QWidget* properties = mainWindow.findChild<QWidget*>(QStringLiteral("featurePropertiesCard"));
    QWidget* activeTool = mainWindow.findChild<QWidget*>(QStringLiteral("activeToolPanel"));
    if (transformAction == nullptr || addBoxAction == nullptr || addCylinderAction == nullptr ||
        properties == nullptr || activeTool == nullptr) {
        qCritical() << "Required desktop controls are missing";
        return false;
    }
    // U05: both float in the viewport and share one compact slot.
    if (properties->parentWidget() != activeTool->parentWidget() || properties->width() > 360 ||
        activeTool->width() > 360) {
        qCritical() << "Properties and tool panel do not share the floating slot";
        return false;
    }
    // The product starts with an empty document: only creating a primitive is possible (U01).
    if (!addBoxAction->isEnabled() || addCylinderAction->isEnabled() ||
        transformAction->isEnabled()) {
        qCritical() << "Empty-document command availability is wrong";
        return false;
    }

    return true;
}
bool verifyCadResize(MainWindow& mainWindow, CadViewport* viewport) {
    mainWindow.resize(mainWindow.width() + 173, mainWindow.height() + 119);
    qInfo() << "smoke-ui resize";
    QCoreApplication::processEvents();
    // Fix the review camera after the model and window dimensions have changed.
    viewport->fitView();
    QCoreApplication::processEvents();
    const auto [renderWidth, renderHeight] = viewport->renderingPixelSize();
    const int expectedWidth =
        static_cast<int>(std::lround(viewport->width() * viewport->devicePixelRatioF()));
    const int expectedHeight =
        static_cast<int>(std::lround(viewport->height() * viewport->devicePixelRatioF()));
    if (std::abs(renderWidth - expectedWidth) > 1 || std::abs(renderHeight - expectedHeight) > 1) {
        return false;
    }

    // A click in empty space must be harmless even though OCCT has no detected owner.
    const QPointF localPosition(1.0, 1.0);
    const QPointF globalPosition(viewport->mapToGlobal(localPosition.toPoint()));
    QMouseEvent emptyClick(QEvent::MouseButtonRelease, localPosition, globalPosition,
                           Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(viewport, &emptyClick);

    return true;
}
bool verifyCleanNavigation(CadSession& session, CadViewport& viewport) {
    if (session.isModified() || session.canUndo())
        return false;
    const QPointF center(viewport.rect().center());
    const QPointF global(viewport.mapToGlobal(center.toPoint()));
    QMouseEvent press(QEvent::MouseButtonPress, center, global, Qt::MiddleButton, Qt::MiddleButton,
                      Qt::NoModifier);
    QMouseEvent move(QEvent::MouseMove, center + QPointF(20, 10), global + QPointF(20, 10),
                     Qt::NoButton, Qt::MiddleButton, Qt::NoModifier);
    QMouseEvent release(QEvent::MouseButtonRelease, center + QPointF(20, 10),
                        global + QPointF(20, 10), Qt::MiddleButton, Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&viewport, &press);
    QCoreApplication::sendEvent(&viewport, &move);
    QCoreApplication::sendEvent(&viewport, &release);
    QWheelEvent wheel(center, global, {}, {0, 120}, Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase,
                      false);
    QCoreApplication::sendEvent(&viewport, &wheel);
    viewport.fitView();
    QCoreApplication::processEvents();
    qInfo() << "smoke-ui clean-navigation";
    return !session.isModified() && !session.canUndo() && !session.canRedo();
}
bool exerciseCadUi(MainWindow& mainWindow, CadSession& session, CadViewport* viewport) {
    if (!verifyCleanNavigation(session, *viewport))
        return false;
    if (!verifyCadControls(mainWindow))
        return false;
    auto* transformAction = mainWindow.findChild<QAction*>(QStringLiteral("transformAction"));
    auto* addBoxAction = mainWindow.findChild<QAction*>(QStringLiteral("addBoxAction"));
    auto* addCylinderAction = mainWindow.findChild<QAction*>(QStringLiteral("addCylinderAction"));
    qInfo() << "smoke-ui box";
    addBoxAction->trigger();
    QCoreApplication::processEvents();
    if (session.currentBodies().size() != 1)
        return false;
    qInfo() << "smoke-ui box-height";
    auto* boxHeight = mainWindow.findChild<QDoubleSpinBox*>(QStringLiteral("boxHeightInput"));
    if (boxHeight == nullptr || !voelund::apps::enterSmokeValue(*boxHeight, QStringLiteral("27")) ||
        std::abs(session.selectedBoxParameters().dimensions.height - 27.0) > 1.0e-6) {
        return false;
    }
    qInfo() << "smoke-ui cylinder";
    if (!addCylinderAction->isEnabled() || !transformAction->isEnabled()) {
        return false;
    }
    const auto previousBodyCount = session.currentBodies().size();
    addCylinderAction->trigger();
    QCoreApplication::processEvents();
    // The connection of the selected cylinder is edited in its properties.
    auto* resultMode = mainWindow.findChild<QComboBox*>(QStringLiteral("cylinderResultModeInput"));
    if (resultMode == nullptr || !resultMode->isVisible())
        return false;
    resultMode->setFocus();
    QKeyEvent keyPress(QEvent::KeyPress, Qt::Key_End, Qt::NoModifier);
    QCoreApplication::sendEvent(resultMode, &keyPress);
    qInfo() << "smoke-ui transform";
    transformAction->trigger();
    QCoreApplication::processEvents();
    if (session.currentBodies().size() != previousBodyCount + 1 ||
        session.selectedCylinderParameters().resultMode !=
            voelund::document::CylinderResultMode::separate) {
        return false;
    }

    return verifyCadResize(mainWindow, viewport);
}
void startCadSmoke(MainWindow& mainWindow, CadSession& session) {
    auto* timer = new QTimer(&mainWindow);
    auto elapsed = std::make_shared<QElapsedTimer>();
    elapsed->start();
    QObject::connect(timer, &QTimer::timeout, &mainWindow, [&mainWindow, &session, timer, elapsed] {
        auto* viewport = dynamic_cast<CadViewport*>(mainWindow.centralWidget());
        if (viewport == nullptr) {
            QCoreApplication::exit(4);
            return;
        }
        if (!voelund::apps::verifyDesktopSmoke(*viewport)) {
            if (elapsed->elapsed() >= 10000)
                QCoreApplication::exit(1);
            return;
        }
        timer->stop();
        if (!exerciseCadUi(mainWindow, session, viewport)) {
            QCoreApplication::exit(8);
            return;
        }
        QTimer::singleShot(150, &mainWindow, [&mainWindow, viewport] {
            QCoreApplication::exit(voelund::apps::verifyDesktopSmoke(*viewport) &&
                                           voelund::apps::captureDesktopSmoke(mainWindow, *viewport)
                                       ? 0
                                       : 9);
        });
    });
    timer->start(50);
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
    QCoreApplication::setApplicationName(QStringLiteral("Völund CAD"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1.0"));
    QCoreApplication::setOrganizationName(QStringLiteral("QuompaCC"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("quompacc.github.io"));
    QGuiApplication::setDesktopFileName(QStringLiteral("io.github.quompacc.Voelund"));
    QApplication application(argc, argv);
    voelund::application::CadSession session;
    session.newDocument();
    const bool isSmokeTest = QCoreApplication::arguments().contains("--smoke-test");
    const bool uninitializedSmoke =
        QCoreApplication::arguments().contains("--smoke-test-uninitialized");
    if (isSmokeTest || uninitializedSmoke)
        voelund::apps::enableSmokeLogging();
    if (uninitializedSmoke) {
        voelund::viewport::CadViewport viewport;
        QTimer::singleShot(0, &application, [&application, &viewport] {
            application.exit(voelund::apps::verifyDesktopSmoke(viewport) ? 0 : 10);
        });
        return application.exec();
    }
    voelund::desktop::MainWindow mainWindow(session);
    mainWindow.show();
    // AI04c: --mcp switches the Claude access (MCP) on, as the chip in the header does; the
    // connect command also goes to the log and to mcp-connect.txt in the configuration
    // directory.
    if (QCoreApplication::arguments().contains(QStringLiteral("--mcp"))) {
        if (mainWindow.setMcpAccessEnabled(true)) {
            const auto directory =
                QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
            QDir().mkpath(directory);
            QFile file(directory + QStringLiteral("/mcp-connect.txt"));
            if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
                file.write(mainWindow.mcpConnectCommand().toUtf8() + '\n');
            qInfo().noquote() << "MCP:" << mainWindow.mcpConnectCommand();
        } else {
            qWarning() << "MCP server could not listen";
        }
    }

    // U07: a document given on the command line (desktop file association) opens through the
    // same safe Open action; a failure shows its error and keeps the new document.
    const auto arguments = QCoreApplication::arguments();
    for (qsizetype index = 1; index < arguments.size(); ++index)
        if (!arguments[index].startsWith(QLatin1Char('-'))) {
            mainWindow.openFile(arguments[index]);
            break;
        }

    if (isSmokeTest)
        startCadSmoke(mainWindow, session);
    return application.exec();
}
