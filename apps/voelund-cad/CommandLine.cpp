#include "CommandLine.hpp"

#include "../../src/desktop/DocumentErrorText.hpp"

#include <voelund/BuildInfo.hpp>
#include <voelund/application/CadSession.hpp>

#include <QCoreApplication>
#include <QStringList>

#include <filesystem>
#include <iostream>
#include <string>

namespace voelund::apps {

namespace {

std::filesystem::path pathOf(const QString& argument) {
#if defined(_WIN32)
    return std::filesystem::path(argument.toStdWString());
#else
    return std::filesystem::path(argument.toStdString());
#endif
}

// Opens `file`, and with a target saves it there and opens the saved copy again.
int checkDocument(const QString& file, const QString& target) {
    application::CadSession session;
    try {
        session.loadDocument(pathOf(file));
    } catch (const std::exception& error) {
        std::cerr << "Abgelehnt: " << desktop::documentOpenErrorText(error.what()).toStdString()
                  << '\n';
        return 2;
    }
    std::cout << "Geöffnet: " << session.features().size() << " Features, "
              << session.validBodies().size() << " gültige Körper\n";
    if (target.isEmpty())
        return 0;
    try {
        session.saveDocument(pathOf(target));
        application::CadSession reopened;
        reopened.loadDocument(pathOf(target));
        std::cout << "Neu gespeichert und geöffnet: " << reopened.features().size()
                  << " Features\n";
        return reopened.features().size() == session.features().size() ? 0 : 3;
    } catch (const std::exception& error) {
        std::cerr << "Speichern fehlgeschlagen: " << error.what() << '\n';
        return 3;
    }
}

} // namespace

std::optional<int> runCommandLine(int argc, char* argv[]) {
    bool wanted = false;
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        wanted = wanted || argument == "--version" || argument == "--check-document";
    }
    if (!wanted)
        return std::nullopt;
    // Arguments as Unicode (Windows passes them in the ANSI code page through argv).
    QCoreApplication application(argc, argv);
    const QStringList arguments = QCoreApplication::arguments();
    if (arguments.contains(QStringLiteral("--version"))) {
        std::cout << "Völund CAD " << build::version << " (" << build::revision << ")\n";
        return 0;
    }
    const auto at = arguments.indexOf(QStringLiteral("--check-document"));
    if (at + 1 >= arguments.size()) {
        std::cerr << "usage: voelund-cad --check-document <file> [--resave <target>]\n";
        return 64;
    }
    const auto resave = arguments.indexOf(QStringLiteral("--resave"));
    return checkDocument(arguments.at(at + 1), resave > 0 && resave + 1 < arguments.size()
                                                   ? arguments.at(resave + 1)
                                                   : QString{});
}

} // namespace voelund::apps
