#include <voelund/desktop/ProductShell.hpp>

#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QStandardPaths>
#include <QSurfaceFormat>

namespace voelund::desktop {

void configureProductApplication() {
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
}

ProductOptions productOptions(const QStringList& arguments) {
    ProductOptions options;
    options.mcp = arguments.contains(QStringLiteral("--mcp"));
    for (qsizetype index = 1; index < arguments.size(); ++index) {
        const QString& argument = arguments[index];
        if (argument.startsWith(QStringLiteral("--smoke-test")))
            options.offerRecovery = false;
        else if (!argument.startsWith(QLatin1Char('-')) && !options.file)
            options.file = argument;
    }
    return options;
}

std::unique_ptr<MainWindow> startProduct(application::CadSession& session,
                                         const ProductOptions& options) {
    session.newDocument();
    auto window = std::make_unique<MainWindow>(session);
    window->show();
    // The connect command also goes to the log and to mcp-connect.txt in the configuration
    // directory.
    if (options.mcp) {
        if (window->setMcpAccessEnabled(true)) {
            const auto directory =
                QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
            QDir().mkpath(directory);
            QFile file(directory + QStringLiteral("/mcp-connect.txt"));
            if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
                file.write(window->mcpConnectCommand().toUtf8() + '\n');
            qInfo().noquote() << "MCP:" << window->mcpConnectCommand();
        } else {
            qWarning() << "MCP server could not listen";
        }
    }
    if (options.offerRecovery)
        window->offerRecovery();
    // The same safe Open action as the file menu; a failure shows its error and keeps the
    // new document.
    if (options.file)
        window->openFile(*options.file);
    return window;
}

} // namespace voelund::desktop
