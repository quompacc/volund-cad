#include "IsolatedSettings.hpp"
#include "UiDriver.hpp"
#include "ViewportLifecycleSupport.hpp"

#include <QApplication>
#include <QComboBox>
#include <QInputDialog>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <string>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>
#include <voelund/desktop/ProductShell.hpp>

// N03 (user decisions 2026-10-22): File > Export with "teil.stl" asks the resolution (medium
// offered), writes both visible bodies into one binary STL and says so. "platte.3mf" with the
// plate picked in the tree offers the last choice (fine) and writes one object "Platte". A
// cancelled resolution question writes nothing.
namespace {
using namespace voelund;
void require(bool condition, const std::string& message) {
    test::require(condition, message.c_str());
}

// Answers the resolution question with `choice` (index), or cancels it (-1); notes what it offered.
struct ResolutionAnswer final {
    int choice{1};
    int asked{0};
    QString offered;
    QTimer timer;
    ResolutionAnswer() {
        QObject::connect(&timer, &QTimer::timeout, [this] {
            auto* dialog = qobject_cast<QInputDialog*>(QApplication::activeModalWidget());
            if (dialog == nullptr ||
                dialog->objectName() != QStringLiteral("meshResolutionQuestion"))
                return;
            ++asked;
            offered = dialog->textValue();
            if (choice < 0) {
                dialog->reject();
                return;
            }
            dialog->setTextValue(dialog->comboBoxItems().at(choice));
            dialog->accept();
        });
        timer.start(30);
    }
};

std::string bytesOf(const QString& file) {
    std::ifstream stream(std::filesystem::path(file.toStdWString()), std::ios::binary);
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

struct Window final {
    application::CadSession session;
    std::unique_ptr<desktop::MainWindow> main;
    QTreeWidget* tree{nullptr};
    Window() {
        desktop::ProductOptions options;
        options.offerRecovery = false;
        main = desktop::startProduct(session, options);
        main->resize(1300, 860);
        tree = main->findChild<QTreeWidget*>(QStringLiteral("modelTree"));
        require(tree != nullptr, "tree");
    }
    void exportFile() {
        test::chooseFromMenu(*main->findChild<QToolButton*>(QStringLiteral("fileMenuButton")),
                             "document.export_step");
        test::settleEvents();
    }
    [[nodiscard]] QString status() const { return main->statusBar()->currentMessage(); }
    void selectBody(const QString& name) {
        for (QTreeWidgetItemIterator it(tree); *it != nullptr; ++it)
            if ((*it)->data(0, Qt::UserRole + 2).toInt() == 3 && (*it)->text(0) == name) {
                tree->setCurrentItem(*it);
                test::settleEvents();
                return;
            }
        throw std::runtime_error("Tree row " + name.toStdString());
    }
};

int run() {
    QTemporaryDir directory;
    require(directory.isValid(), "temporary directory");
    test::DialogAnswers user;
    ResolutionAnswer resolution;
    Window w;
    test::activate(*w.main);
    const auto plate = w.session.createBoxFeature({40, 20, 10}).feature();
    require(plate && w.session.renameBody(document::BodyId{plate->value()}, "Platte").status() ==
                         application::CommandStatus::committed,
            "Plate");
    require(w.session.createNewBodyCylinder().status() == application::CommandStatus::committed,
            "Cylinder");
    w.main->refreshAfterExternalChange();
    test::settleEvents();

    user.file = directory.path() + QStringLiteral("/teil.stl");
    resolution.choice = 2;
    w.exportFile();
    const auto stl = bytesOf(user.file);
    require(resolution.asked == 1 && resolution.offered.startsWith(QStringLiteral("Mittel")) &&
                stl.size() > 84 && w.status().contains(QStringLiteral("STL")) &&
                w.status().contains(QStringLiteral("2 sichtbare")),
            "STL of both visible bodies after the question: " + w.status().toStdString());

    w.selectBody(QStringLiteral("Platte"));
    user.file = directory.path() + QStringLiteral("/platte.3mf");
    resolution.choice = 1;
    w.exportFile();
    const auto package = bytesOf(user.file);
    require(resolution.asked == 2 && resolution.offered.startsWith(QStringLiteral("Fein")) &&
                package.find("name=\"Platte\"") != std::string::npos &&
                w.status().contains(QStringLiteral("1 gewählte")),
            "3MF of the picked plate, the last choice offered: " + w.status().toStdString());

    user.file = directory.path() + QStringLiteral("/nichts.stl");
    resolution.choice = -1;
    w.exportFile();
    require(resolution.asked == 3 &&
                !std::filesystem::exists(std::filesystem::path(user.file.toStdWString())),
            "A cancelled question writes nothing");
    return 0;
}
} // namespace

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    voelund::test::isolateSettings();
    try {
        const int result = run();
        std::cout << "PASS N03 mesh export through the shell: STL of the visible bodies after "
                     "the resolution question, 3MF of the picked body with the last choice, "
                     "cancel writes nothing"
                  << std::endl;
        return result;
    } catch (const std::exception& error) {
        std::cerr << error.what() << std::endl;
        return 1;
    }
}
