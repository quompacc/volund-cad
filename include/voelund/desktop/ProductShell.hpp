#pragma once

#include <QString>
#include <QStringList>

#include <memory>
#include <optional>

namespace voelund::application {
class CadSession;
}

namespace voelund::desktop {

class MainWindow;

// U12: the composition root of the product. `voelund-cad` and the UI flow tests start the
// same shell through it: the application identity and surface format before the
// QApplication exists, then one window on one session with the command-line options.
void configureProductApplication();

struct ProductOptions final {
    // AI04c: switch the Claude access (MCP) on, as the chip in the header does.
    bool mcp{false};
    // L05: offer unsaved work of an interrupted run first.
    bool offerRecovery{true};
    // U07: a document given on the command line (desktop file association).
    std::optional<QString> file;
    // L02: further documents on the command line. A window holds one document, so they are
    // named in the status bar instead of being opened.
    QStringList ignoredFiles;
};

// Smoke-test switches (`--smoke-test…`) start from a clean document without recovery.
[[nodiscard]] ProductOptions productOptions(const QStringList& arguments);

// Starts a new document on `session`, shows the shell and applies `options`. The session
// must outlive the window.
[[nodiscard]] std::unique_ptr<MainWindow> startProduct(application::CadSession& session,
                                                       const ProductOptions& options);

} // namespace voelund::desktop
