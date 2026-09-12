#include "PreviewStyle.hpp"
#include "PreviewWindow.hpp"

#include <QApplication>
#include <QTimer>

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
    application.setApplicationDisplayName(QStringLiteral("Völund UI Preview"));
    application.setOrganizationName(QStringLiteral("Völund"));
    application.setStyleSheet(voelund::preview::applicationStyle());

    voelund::preview::PreviewWindow window;
    window.show();

    if (application.arguments().contains(QStringLiteral("--smoke-test"))) {
        QTimer::singleShot(250, &application, [&application, &window] {
            application.exit(window.smokeTestPassed() ? 0 : 1);
        });
    }

    return application.exec();
}
