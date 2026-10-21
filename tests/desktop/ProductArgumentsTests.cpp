#include "IsolatedSettings.hpp"
#include "UiDriver.hpp"
#include "ViewportLifecycleSupport.hpp"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTimer>
#include <iostream>
#include <memory>
#include <string>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>
#include <voelund/desktop/ProductShell.hpp>

// L02: documents on the command line, as a file association passes them. The first opens,
// also from a folder and file name with umlauts and spaces; further ones are not opened but
// named in the status bar.
namespace {
using namespace voelund;

void require(bool condition, const std::string& message) {
    test::require(condition, message.c_str());
}

void parsing() {
    const auto options = desktop::productOptions(
        {QStringLiteral("voelund-cad"), QStringLiteral("--mcp"), QStringLiteral("a.volund"),
         QStringLiteral("--smoke-test"), QStringLiteral("b.volund"),
         QStringLiteral("Prüf Ordner/c d.volund")});
    require(options.mcp && !options.offerRecovery, "Switches are read wherever they stand");
    require(options.file == QStringLiteral("a.volund"), "The first document is the one to open");
    require(options.ignoredFiles ==
                QStringList({QStringLiteral("b.volund"), QStringLiteral("Prüf Ordner/c d.volund")}),
            "Further documents are kept to be named");
}

void opening(const QString& fixture) {
    QTemporaryDir root;
    require(root.isValid(), "Temporary directory");
    const QString folder = root.filePath(QStringLiteral("Prüf Ordner äöü"));
    require(QDir().mkpath(folder), "Folder with umlauts and spaces");
    const QString document = folder + QStringLiteral("/Teil mit Ä.volund");
    require(QFile::copy(fixture, document), "Fixture copied");

    desktop::ProductOptions options;
    options.offerRecovery = false;
    options.file = document;
    options.ignoredFiles = {folder + QStringLiteral("/Zweites.volund"),
                            QStringLiteral("drittes Teil.volund")};
    application::CadSession session;
    const auto window = desktop::startProduct(session, options);
    test::settleEvents();
    // N16: the programme symbol for window and task bar.
    require(!QGuiApplication::windowIcon().isNull() &&
                QGuiApplication::windowIcon().availableSizes().contains(QSize(256, 256)),
            "The programme symbol is the window icon");
    require(!session.features().empty() &&
                QFileInfo(QString::fromStdU16String(session.currentFile().u16string()))
                        .canonicalFilePath() == QFileInfo(document).canonicalFilePath(),
            "The document under a path with umlauts and spaces is open");
    const QString message = window->statusBar()->currentMessage();
    require(message.contains(QStringLiteral("nicht geöffnet")) &&
                message.contains(QStringLiteral("Zweites.volund")) &&
                message.contains(QStringLiteral("drittes Teil.volund")),
            "The status bar names the documents that were not opened");
}

int run(const QString& fixture) {
    parsing();
    opening(fixture);
    std::cout << "PASS product arguments: first document opens (umlauts, spaces), further "
                 "ones are named\n";
    return 0;
}
} // namespace

int main(int argc, char** argv) {
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    voelund::desktop::configureProductApplication();
    QApplication application(argc, argv);
    voelund::test::isolateSettings();
    if (argc != 2) {
        std::cerr << "usage: voelund_product_arguments_tests <fixture.volund>\n";
        return 2;
    }
    const QString fixture = QString::fromLocal8Bit(argv[1]);
    int result = 1;
    QTimer::singleShot(0, [&result, &fixture] {
        try {
            result = run(fixture);
        } catch (const std::exception& error) {
            std::cerr << error.what() << '\n';
            result = 1;
        }
        QApplication::exit(result);
    });
    application.exec();
    return result;
}
