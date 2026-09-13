#include "PreviewStyle.hpp"
#include "PreviewWindow.hpp"
#include "SketchBackend.hpp"

#include <voelund/application/CadSession.hpp>

#include <QApplication>
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
    QApplication application(argc, argv);
    application.setApplicationDisplayName(QStringLiteral("Völund UI Preview"));
    application.setOrganizationName(QStringLiteral("Völund"));
    application.setStyleSheet(voelund::preview::applicationStyle());

    voelund::application::CadSession session;
    const bool smokeTest = application.arguments().contains(QStringLiteral("--smoke-test"));
    if (smokeTest) {
        voelund::preview::SketchBackend backend(session);
        using SketchTool = voelund::preview::PreviewCanvas::SketchTool;
        using Intent = voelund::viewport::SketchElementIntent;
        if (!backend.begin()) return 2;
        backend.setTool(SketchTool::Line);
        const bool line = backend.confirmElement(
            Intent{-10.0, -8.0, 10.0, -8.0, std::nullopt, std::nullopt, std::nullopt,
                   std::nullopt, std::nullopt, std::nullopt});
        backend.setTool(SketchTool::Rectangle);
        const bool rectangle = backend.confirmElement(
            Intent{-8.0, -5.0, 8.0, 5.0, std::nullopt, std::nullopt, std::nullopt,
                   std::nullopt, std::nullopt, std::nullopt});
        backend.setTool(SketchTool::Circle);
        const bool circle = backend.confirmElement(
            Intent{20.0, 10.0, 25.0, 10.0, std::nullopt, std::nullopt, std::nullopt,
                   std::nullopt, std::nullopt, std::nullopt});
        backend.setTool(SketchTool::Arc);
        const bool arc = backend.confirmElement(
            Intent{30.0, 10.0, 35.0, 10.0, std::pair{30.0, 15.0}, std::nullopt,
                   std::nullopt, std::nullopt, std::nullopt, std::nullopt});
        if (!line || !rectangle || !circle || !arc || backend.sceneElements().empty()
            || !backend.finish()) {
            return 2;
        }
    }
    voelund::preview::PreviewWindow window(session);
    window.show();

    if (smokeTest) {
        QTimer::singleShot(250, &application, [&application, &window] {
            application.exit(window.smokeTestPassed() ? 0 : 1);
        });
    }

    return application.exec();
}
