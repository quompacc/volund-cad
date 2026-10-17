#include "IsolatedSettings.hpp"
#include "UiDriver.hpp"
#include "ViewportLifecycleSupport.hpp"

#include <BRepAdaptor_Curve.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRep_Builder.hxx>
#include <QApplication>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <STEPControl_Writer.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>
#include <voelund/desktop/ProductShell.hpp>
#include <voelund/kernel/StepExchange.hpp>

// M18 (A-STEP-01 and A-ERR-01 through the shell; user decisions 2026-10-16): File > Export
// writes the three visible bodies, or the body picked in the tree alone; the status bar says
// so. A model with an error is not exported: an error message, no file dialog, no file. File >
// Import puts the file into the open document as one timeline step, one undo; a file in inches
// says so; a broken file shows an error and changes nothing; a file with loose faces besides a
// solid imports the solid and explains the rest. File > Open takes a STEP file as a new
// document after the unsaved-changes question. Saved, the STEP file removed, the document opens
// with its bodies.
namespace {
using namespace voelund;
void require(bool condition, const std::string& message) {
    test::require(condition, message.c_str());
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
        auto* viewport = dynamic_cast<viewport::CadViewport*>(main->centralWidget());
        tree = main->findChild<QTreeWidget*>(QStringLiteral("modelTree"));
        require(viewport != nullptr && tree != nullptr, "viewport and tree");
        test::render(*viewport);
    }
    void file(const char* id) {
        test::chooseFromMenu(*main->findChild<QToolButton*>(QStringLiteral("fileMenuButton")), id);
        test::settleEvents();
    }
    [[nodiscard]] QString status() const { return main->statusBar()->currentMessage(); }
    [[nodiscard]] int steps() const {
        int count = 0;
        for (auto* step : main->findChildren<QToolButton*>(QStringLiteral("timelineStep")))
            count += step->isVisible() ? 1 : 0;
        return count;
    }
    void selectBody(const QString& name) {
        for (QTreeWidgetItemIterator it(tree); *it != nullptr; ++it)
            if ((*it)->data(0, Qt::UserRole + 2).toInt() == 3 && (*it)->text(0) == name) {
                tree->setCurrentItem(*it);
                test::settleEvents();
                return;
            }
        throw std::runtime_error("Tree row " + name.toStdString());
    }
    void refresh() {
        main->refreshAfterExternalChange();
        test::settleEvents();
    }
};

std::filesystem::path path(const QString& file) {
    return std::filesystem::path(file.toStdWString());
}

int run() {
    QTemporaryDir directory;
    require(directory.isValid(), "temporary directory");
    const QString folder = directory.path();
    test::DialogAnswers user;
    Window w;
    test::activate(*w.main);

    // A plate "Platte", its pattern copy and a cylinder (set up directly; the exchange is
    // what this test drives through the shell).
    const auto plate = w.session.createBoxFeature({40, 20, 10}).feature();
    require(plate.has_value() &&
                w.session.renameBody(document::BodyId{plate->value()}, "Platte").status() ==
                    application::CommandStatus::committed,
            "Plate");
    application::PatternInput pattern;
    pattern.baseAxis = document::BaseAxis::x;
    pattern.spacing = 60;
    require(w.session.createPattern(*plate, pattern).status() ==
                application::CommandStatus::committed,
            "Pattern");
    require(w.session.createNewBodyCylinder().status() == application::CommandStatus::committed,
            "Cylinder");
    w.refresh();

    // Export of the visible bodies.
    user.file = folder + QStringLiteral("/teil.step");
    w.file("document.export_step");
    require(user.fileDialogs == 1 && std::filesystem::exists(path(user.file)) &&
                w.status().contains(QStringLiteral("3 sichtbare")),
            "Export writes three visible bodies: " + w.status().toStdString());
    // The body picked in the tree alone.
    w.selectBody(QStringLiteral("Platte"));
    user.file = folder + QStringLiteral("/platte.step");
    w.file("document.export_step");
    const auto platteOnly = kernel::readStep(path(user.file));
    require(w.status().contains(QStringLiteral("1 gewählte")) && platteOnly.solids.size() == 1 &&
                platteOnly.solids[0].name == "Platte",
            "The picked body is exported alone: " + w.status().toStdString());
    w.tree->clearSelection();
    w.tree->setCurrentItem(nullptr);
    test::settleEvents();

    // A-ERR-01: a rounding made impossible is kept with its error; no export, not even of the
    // last valid display.
    std::optional<document::FilletEdge> edge;
    for (TopExp_Explorer explorer(*w.session.featureShape(*plate), TopAbs_EDGE); explorer.More();
         explorer.Next()) {
        const BRepAdaptor_Curve curve(TopoDS::Edge(explorer.Current()));
        const auto middle = curve.Value((curve.FirstParameter() + curve.LastParameter()) / 2);
        if (std::abs(middle.Y()) < 1e-9 && std::abs(middle.Z() - 10) < 1e-9)
            edge = w.session.filletEdge(*plate, explorer.Current());
    }
    require(edge.has_value(), "Plate edge");
    const auto fillet = w.session.createFillet(*plate, std::span{&*edge, 1}, 2).feature();
    require(fillet.has_value(), "Fillet");
    auto radius = w.session.filletParameters(*fillet);
    radius.radius = 50;
    require(w.session
                    .editFilletParameters(*fillet, radius,
                                          application::ErrorDisposition::keepEditableErrors)
                    .status() == application::CommandStatus::committedWithErrors,
            "Rounding with an error");
    w.refresh();
    const int dialogs = user.fileDialogs;
    const int errors = user.errors;
    user.file = folder + QStringLiteral("/fehler.step");
    w.file("document.export_step");
    require(user.errors == errors + 1 && user.fileDialogs == dialogs &&
                !std::filesystem::exists(path(user.file)),
            "A model with an error is not exported");

    // Import into the open document: one step, one undo.
    user.answer = QMessageBox::Discard;
    w.file("document.new");
    require(w.session.features().empty(), "New document");
    user.file = folder + QStringLiteral("/teil.step");
    w.file("document.import_step");
    require(w.session.validBodies().size() == 3 && w.steps() == 1 &&
                w.status().contains(QStringLiteral("Importiert: 3 Körper")),
            "Import: three bodies, one timeline step: " + w.status().toStdString());
    require(w.session.undo().status() == application::CommandStatus::committed, "Undo");
    w.refresh();
    require(w.session.features().empty() && w.steps() == 0, "The import was one undo step");

    // A file in inches says so.
    const auto inches = folder + QStringLiteral("/zoll.step");
    const kernel::StepSolid cube{BRepPrimAPI_MakeBox(25.4, 25.4, 25.4).Solid(), "Würfel"};
    require(
        !kernel::writeStep(path(inches), std::span{&cube, 1}, kernel::StepLengthUnit::inch).error,
        "Inch file");
    user.file = inches;
    w.file("document.import_step");
    require(w.status().contains(QStringLiteral("Zoll")),
            "Inches named: " + w.status().toStdString());

    // A broken file: an error, nothing changes.
    const auto before = w.session.features();
    const auto broken = folder + QStringLiteral("/kaputt.step");
    std::ofstream(path(broken)) << "ISO-10303-21;\nHEADER;\n";
    user.file = broken;
    const int errorsBefore = user.errors;
    w.file("document.import_step");
    require(user.errors == errorsBefore + 1 && w.session.features() == before,
            "A broken file shows an error and changes nothing");

    // A solid with a loose face: imported, the face explained.
    const auto mixed = folder + QStringLiteral("/gemischt.step");
    {
        TopoDS_Compound compound;
        BRep_Builder builder;
        builder.MakeCompound(compound);
        builder.Add(compound, BRepPrimAPI_MakeBox(gp_Pnt(100, 0, 0), 5, 5, 5).Solid());
        BRepBuilderAPI_MakePolygon square(gp_Pnt(0, 100, 0), gp_Pnt(10, 100, 0), gp_Pnt(10, 110, 0),
                                          gp_Pnt(0, 110, 0), true);
        builder.Add(compound, BRepBuilderAPI_MakeFace(square.Wire()).Face());
        STEPControl_Writer writer;
        require(writer.Transfer(compound, STEPControl_AsIs) == IFSelect_RetDone &&
                    writer.Write(path(mixed).string().c_str()) == IFSelect_RetDone,
                "Mixed file");
    }
    user.file = mixed;
    const int hintsBefore = user.errors;
    w.file("document.import_step");
    require(user.errors == hintsBefore + 1 && w.session.validBodies().size() == 2,
            "The solid is imported, the loose face explained");

    // Open takes a STEP file as a new document, after the unsaved-changes question.
    const int prompts = user.prompts;
    user.file = folder + QStringLiteral("/teil.step");
    w.file("document.open");
    require(user.prompts == prompts + 1 && w.session.currentFile().empty() &&
                w.session.validBodies().size() == 3 && w.steps() == 1,
            "Open makes a new document of the STEP file");

    // Saved, the STEP file gone, the document still has its bodies.
    user.file = folder + QStringLiteral("/import.volund");
    w.file("document.save_as");
    std::filesystem::remove(path(folder + QStringLiteral("/teil.step")));
    w.file("document.new");
    w.file("document.open");
    require(w.session.validBodies().size() == 3 && w.steps() == 1,
            "The saved import opens without the STEP file");
    return 0;
}
} // namespace

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    voelund::test::isolateSettings();
    try {
        const int result = run();
        std::cout << "PASS M18 STEP exchange through the shell: export of visible and picked "
                     "bodies, refused for a model with an error, import as one step and one "
                     "undo, inches named, broken file without change, loose face explained, "
                     "Open of a STEP file, saved import without its file"
                  << std::endl;
        return result;
    } catch (const std::exception& error) {
        std::cerr << error.what() << std::endl;
        return 1;
    }
}
