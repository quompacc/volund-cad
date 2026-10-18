#include "IsolatedSettings.hpp"
#include "UiDriver.hpp"
#include "ViewportLifecycleSupport.hpp"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QDialog>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTimer>
#include <iostream>
#include <string>
#include <voelund/BuildInfo.hpp>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>
#include <voelund/desktop/ProductShell.hpp>

// L03 (user decision 2026-10-18): "Über Völund…" in the main menu shows version, revision,
// document schema and library versions, and copies them as one text.
namespace {
using namespace voelund;

void require(bool condition, const std::string& message) {
    test::require(condition, message.c_str());
}

int run() {
    application::CadSession session;
    desktop::ProductOptions options;
    options.offerRecovery = false;
    const auto window = desktop::startProduct(session, options);
    auto* about = window->findChild<QAction*>(QStringLiteral("aboutAction"));
    auto* menu = window->findChild<QMenu*>(QStringLiteral("fileMenu"));
    require(about != nullptr && menu != nullptr && menu->actions().contains(about) &&
                menu->actions().back() == about,
            "\"Über Völund…\" is the last entry of the main menu");
    QString shown;
    QString copied;
    QTimer::singleShot(0, [&] {
        auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        require(dialog != nullptr && dialog->objectName() == QStringLiteral("aboutDialog"),
                "The about dialog opens");
        shown =
            dialog->findChild<QPlainTextEdit*>(QStringLiteral("aboutDiagnostics"))->toPlainText();
        dialog->findChild<QPushButton*>(QStringLiteral("aboutCopyDiagnostics"))->click();
        copied = QGuiApplication::clipboard()->text();
        dialog->reject();
    });
    about->trigger();
    const QString version = QString::fromLatin1(build::version);
    require(shown.startsWith(QStringLiteral("Völund CAD ") + version) &&
                shown.contains(QString::fromLatin1(build::revision)) &&
                shown.contains(QStringLiteral("Schema ")) &&
                shown.contains(QStringLiteral("OpenCASCADE")) &&
                shown.contains(QStringLiteral("Qt")),
            "The dialog names version, revision, schema and libraries");
    require(copied == shown, "\"Diagnose kopieren\" copies the whole text");
    require(QCoreApplication::applicationVersion() == version,
            "The application reports the build's version");
    std::cout << "PASS about dialog: version " << build::version << ", revision " << build::revision
              << '\n';
    return 0;
}
} // namespace

int main(int argc, char** argv) {
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    voelund::desktop::configureProductApplication();
    QApplication application(argc, argv);
    voelund::test::isolateSettings();
    int result = 1;
    QTimer::singleShot(0, [&result] {
        try {
            result = run();
        } catch (const std::exception& error) {
            std::cerr << error.what() << '\n';
            result = 1;
        }
        QApplication::exit(result);
    });
    application.exec();
    return result;
}
