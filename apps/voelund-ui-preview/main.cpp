#include "PreviewStyle.hpp"
#include "PreviewWindow.hpp"
#include "SketchBackend.hpp"

#include <voelund/application/CadSession.hpp>

#include <QApplication>
#include <QTimer>

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
    application.setApplicationDisplayName(QStringLiteral("Völund UI Preview"));
    application.setOrganizationName(QStringLiteral("Völund"));
    application.setStyleSheet(voelund::preview::applicationStyle());

    voelund::application::CadSession session;
    const bool smokeTest = application.arguments().contains(QStringLiteral("--smoke-test"));
    if (smokeTest) {
        voelund::preview::SketchBackend backend(session);
        if (!backend.begin()
            || !backend.add(voelund::preview::PreviewCanvas::SketchTool::Rectangle,
                            QPointF(-10.0, -8.0), QPointF(10.0, 8.0))
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
