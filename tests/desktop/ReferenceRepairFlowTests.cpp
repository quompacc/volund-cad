#include "IsolatedSettings.hpp"
#include "../../src/desktop/ReferenceRepairDialog.hpp"
#include "../BoundSketchFixture.hpp"
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QElapsedTimer>
#include <QFileDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMouseEvent>
#include <QPushButton>
#include <QThread>
#include <QTimer>
#include <fstream>
#include <iostream>
#include <source_location>
#include <voelund/desktop/MainWindow.hpp>
#include <voelund/document/DocumentSerializer.hpp>
#include <voelund/viewport/CadViewport.hpp>
using namespace voelund;
using namespace document;
using application::CommandStatus;
namespace {
void require(bool value, const char* message,
             const std::source_location where = std::source_location::current()) {
    if (!value)
        throw std::runtime_error(std::string(message) + " at " + std::to_string(where.line()));
}
void events() { QApplication::processEvents(); }
void click(QWidget* widget, QPoint at = {}) {
    require(widget && widget->isEnabled(), "Disabled/missing control");
    if (at.isNull())
        at = widget->rect().center();
    QMouseEvent down(QEvent::MouseButtonPress, at, widget->mapToGlobal(at), Qt::LeftButton,
                     Qt::LeftButton, Qt::NoModifier);
    QMouseEvent up(QEvent::MouseButtonRelease, at, widget->mapToGlobal(at), Qt::LeftButton,
                   Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(widget, &down);
    QApplication::sendEvent(widget, &up);
    events();
}
QAction* action(QWidget& w, const QString& name) {
    for (auto* a : w.findChildren<QAction*>())
        if (a->property("commandId").toString() == name || a->objectName() == name)
            return a;
    throw std::runtime_error("Missing action: " + name.toStdString());
}
desktop::ReferenceRepairDialog* open(desktop::MainWindow& w) {
    auto* a = action(w, "repairReferencesAction");
    require(a->isEnabled(), "Repair action disabled");
    a->trigger();
    events();
    for (auto* d : w.findChildren<QDialog*>())
        if (d->isVisible() && d->objectName() == "referenceRepairDialog")
            return dynamic_cast<desktop::ReferenceRepairDialog*>(d);
    throw std::runtime_error("Repair dialog missing");
}
QListWidget* list(QWidget& d) { return d.findChild<QListWidget*>("repairCandidates"); }
QPushButton* apply(QWidget& d) { return d.findChild<QPushButton*>("repairApply"); }
void choose(QWidget& d, int row) {
    auto* l = list(d);
    require(row >= 0 && row < l->count(), "Candidate missing");
    l->scrollToItem(l->item(row));
    events();
    click(l->viewport(), l->visualItemRect(l->item(row)).center());
    require(l->currentRow() == row && apply(d)->isEnabled(), "Candidate not selected");
}
void cancel(QWidget& d) {
    click(d.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Cancel));
}
void capture(QWidget& w, const QString& name) {
    const auto dir = qEnvironmentVariable("VOELUND_REFERENCE_REPAIR_ARTIFACT_DIR");
    if (dir.isEmpty())
        return;
    require(QDir().mkpath(dir), "Capture directory");
    events();
    require(w.grab().save(dir + "/" + name + ".png"), "Capture failed");
}
ModelDocument fixture(bool edges, bool multiple) {
    auto m = ModelDocument::empty();
    const auto b = m.addBoxFeature("Support", {20, 20, 10});
    auto support = b;
    double z = 10;
    if (multiple) {
        SketchParameters base{{0, 0, 10, 0, 0, 1, 1, 0, 0},
                              {{{SketchElementId{1}, SketchRectangle{2, 2, 5, 5}},
                                {SketchElementId{2}, SketchRectangle{10, 10, 13, 13}}},
                               3},
                              {}};
        auto s = m.addSketchFeature("Two profiles", b, base);
        test::bindTop(m, s);
        support =
            m.addExtrusionFeature("Two caps", s, {3, {SketchProfileId{1}, SketchProfileId{2}}});
        z = 13;
    }
    SketchParameters p{{0, 0, z, 0, 0, 1, 1, 0, 0},
                       {{{SketchElementId{1}, SketchRectangle{3, 3, 4, 4}},
                         {SketchElementId{2}, SketchLine{{2, 2}, {7, 7}}, true}},
                        3},
                       {}};
    // Attachment helper is construction, not a branching material boundary.
    if (edges)
        for (auto kind : {SketchConstraintKind::coordinateX, SketchConstraintKind::coordinateY})
            p.constraints.values.push_back({SketchConstraintId{p.constraints.nextId++},
                                            kind,
                                            {SketchElementId{2}, SketchPointPosition::start},
                                            std::nullopt,
                                            2,
                                            std::nullopt,
                                            {3, .5}});
    auto legacy = m.addSketchFeature("Legacy reference", support, p);
    (void)m.addExtrusionFeature("Dependent pad", legacy, {2, {SketchProfileId{1}}});
    (void)m.addBoxFeature("Independent", {2, 2, 2});
    return m;
}
void write(const ModelDocument& m, const std::filesystem::path& p) {
    std::ofstream out(p, std::ios::binary);
    DocumentSerializer::write(m, out);
}
void ready(desktop::MainWindow& w) {
    w.resize(1400, 900);
    w.show();
    auto* v = dynamic_cast<viewport::CadViewport*>(w.centralWidget());
    QElapsedTimer timer;
    timer.start();
    while (!v->isRenderingReady() && timer.elapsed() < 10000) {
        events();
        QThread::msleep(10);
    }
    require(v->isRenderingReady(), "Renderer not ready");
    events();
}
void fileAction(desktop::MainWindow& w, const QString& name, const std::filesystem::path& path) {
    bool answered = false;
    QTimer timer;
    QObject::connect(&timer, &QTimer::timeout, [&] {
        if (auto* d = qobject_cast<QFileDialog*>(QApplication::activeModalWidget())) {
#ifdef _WIN32
            const auto p = QString::fromStdWString(path.wstring());
#else
            const auto p=QString::fromStdString(path.string());
#endif
            d->selectFile(p);
            if (auto* edit = d->findChild<QLineEdit*>("fileNameEdit"))
                edit->setText(p);
            answered = true;
            QMetaObject::invokeMethod(d, "accept", Qt::DirectConnection);
        }
    });
    timer.start(30);
    action(w, name)->trigger();
    require(answered, "File dialog not exercised");
    events();
}
void supportFlow(const std::filesystem::path& directory, bool multiple) {
    const auto path =
        directory / (multiple ? "repair-ui-multiple-source.volund" : "repair-ui-source.volund");
    write(fixture(false, multiple), path);
    application::CadSession session;
    session.loadDocument(path);
    const FeatureId sketch{multiple ? 4u : 2u};
    require(session.selectFeature(sketch), "Sketch selection");
    const auto original = session.identifiedSnapshot();
    const auto stamp = session.sceneStatus().document;
    desktop::MainWindow w(session);
    ready(w);
    auto* d = open(w);
    require(!apply(*d)->isEnabled() && list(*d)->currentRow() == -1, "Automatic selection");
    const auto inspection = session.inspectSketchReferences(sketch);
    const auto matches = std::ranges::count_if(
        inspection.review.candidates, [](const auto& c) { return c.matchesOriginalPlane; });
    require(matches == (multiple ? 2 : 1), "Wrong matching fixture count");
    int row = -1;
    for (std::size_t i = 0; i < inspection.review.candidates.size(); ++i)
        if (inspection.review.candidates[i].matchesOriginalPlane) {
            row = static_cast<int>(i);
            break;
        }
    choose(*d, row);
    if (multiple) {
        const auto first = d->findChild<QLabel*>("repairPreviewCoordinates")->text();
        int alternative = -1;
        for (std::size_t i = static_cast<std::size_t>(row + 1);
             i < inspection.review.candidates.size(); ++i)
            if (inspection.review.candidates[i].matchesOriginalPlane) {
                alternative = static_cast<int>(i);
                break;
            }
        choose(*d, alternative);
        require(d->findChild<QLabel*>("repairPreviewCoordinates")->text() != first,
                "Coplanar alternatives are not distinguishable in preview");
        capture(*d, "multiple-alternative");
        choose(*d, row);
    }
    capture(*d, multiple ? "multiple-candidates" : "single-candidate");
    if (!multiple) {
        d->resize(620, 640);
        events();
        capture(*d, "single-narrow");
        require(apply(*d)->isVisible() && apply(*d)->isEnabled(), "Narrow dialog lost Apply");
    }
    cancel(*d);
    require(session.identifiedSnapshot() == original && session.sceneStatus().document == stamp &&
                !session.isModified() && !session.canUndo(),
            "Cancel mutated state");
    d = open(w);
    choose(*d, row);
    click(apply(*d));
    require(!session.sketchParameters(sketch).repairReason && session.sceneStatus().isCurrent(),
            "UI repair failed");
    const auto repaired = session.identifiedSnapshot();
    capture(w, multiple ? "multiple-repaired" : "single-repaired");
    action(w, "history.undo")->trigger();
    events();
    require(session.identifiedSnapshot() == original && !session.sceneStatus().isCurrent(),
            "Undo lost original error");
    capture(w, multiple ? "multiple-undo" : "single-undo");
    action(w, "history.redo")->trigger();
    events();
    require(session.identifiedSnapshot() == repaired, "Redo lost repair");
    const auto saved =
        directory / (multiple ? "repair-ui-multiple-fixed.volund" : "repair-ui-fixed.volund");
    std::filesystem::remove(saved);
    fileAction(w, "document.save_as", saved);
    fileAction(w, "document.open", saved);
    require(session.identifiedSnapshot() == repaired && session.sceneStatus().isCurrent() &&
                !session.isModified(),
            "UI Save/Open failed");
    application::CadSession untouched;
    untouched.loadDocument(path);
    require(untouched.identifiedSnapshot() == original, "Source file changed without Save");
    std::cout << (multiple ? "two coplanar candidates" : "single geometric proposal")
              << ": explicit UI selection, Cancel, Undo/Redo, file dialogs passed\n";
}
void edgeAndStale(const std::filesystem::path& directory) {
    const auto path = directory / "repair-ui-edges-source.volund";
    write(fixture(true, false), path);
    application::CadSession s;
    s.loadDocument(path);
    require(s.selectFeature(FeatureId{2}), "Select sketch");
    desktop::MainWindow w(s);
    ready(w);
    auto* d = open(w);
    const auto inspection = s.inspectSketchReferences(FeatureId{2});
    int row = -1;
    for (std::size_t i = 0; i < inspection.review.candidates.size(); ++i)
        if (inspection.review.candidates[i].matchesOriginalPlane) {
            row = static_cast<int>(i);
            break;
        }
    choose(*d, row);
    const auto original = s.identifiedSnapshot();
    click(apply(*d));
    require(s.identifiedSnapshot() == original, "Implicit remaining-error commit");
    auto* keep = d->findChild<QCheckBox*>("repairKeepErrors");
    require(keep->isVisible() && !keep->isChecked(), "Missing explicit error choice");
    capture(*d, "partial-repair-error");
    click(keep, QPoint(8, keep->height() / 2));
    require(keep->isChecked(), "Error choice did not toggle");
    click(apply(*d));
    require(s.sketchParameters(FeatureId{2}).repairReason == SketchRepairReason::legacyEdges,
            "Partial state missing");
    d = open(w);
    auto* target = d->findChild<QComboBox*>("repairTarget");
    target->setCurrentIndex(1);
    events();
    require(list(*d)->count() > 0 && list(*d)->currentRow() == -1, "Edge proposals preselected");
    const auto projection =
        s.projectSketchSupport(FeatureId{1}, *s.sketchParameters(FeatureId{2}).supportBinding);
    const auto& c = projection.points.front();
    require(c.reference.document == s.documentId(), "Edge namespace");
    choose(*d, 0);
    capture(*d, "edge-candidate");
    click(apply(*d));
    require(s.sceneStatus().isCurrent() && !s.sketchParameters(FeatureId{2}).repairReason,
            "Edge repair failed");
    const auto fixed = s.identifiedSnapshot();
    action(w, "history.undo")->trigger();
    events();
    require(s.sketchParameters(FeatureId{2}).repairReason == SketchRepairReason::legacyEdges,
            "Edge undo");
    action(w, "history.redo")->trigger();
    events();
    require(s.identifiedSnapshot() == fixed, "Edge redo");
    d = open(w);
    choose(*d, row);
    require(s.selectFeature(FeatureId{4}), "Select independent");
    auto box = s.selectedBoxParameters();
    box.dimensions.width = 3;
    require(s.editSelectedBoxParameters(box).status() == CommandStatus::committed,
            "Independent edit failed");
    require(s.selectFeature(FeatureId{2}), "Restore selection");
    const auto changed = s.identifiedSnapshot();
    click(apply(*d));
    require(s.identifiedSnapshot() == changed && !apply(*d)->isEnabled() &&
                d->findChild<QLabel*>("repairMessage")->text().contains("stale"),
            "Stale UI committed");
    capture(*d, "stale-choice");
    click(d->findChild<QPushButton*>("repairRefresh"));
    require(!apply(*d)->isEnabled() && list(*d)->currentRow() == -1, "Refresh retained selection");
    cancel(*d);
    s.saveDocument(directory / "repair-ui-edges-fixed.volund");
    std::cout << "edge group, explicit partial repair, independent edit and stale refresh passed\n";
}
void unavailable(const std::filesystem::path& directory) {
    auto model = fixture(false, false);
    const auto sketch =
        model.addSketchFeature("Blocked support sketch", FeatureId{3},
                               {{0, 0, 12, 0, 0, 1, 1, 0, 0},
                                {{{SketchElementId{1}, SketchLine{{3, 3}, {4, 4}}}}, 2},
                                {}});
    const auto path = directory / "repair-ui-unavailable.volund";
    write(model, path);
    application::CadSession s;
    s.loadDocument(path);
    require(s.selectFeature(sketch), "Select unavailable sketch");
    desktop::MainWindow w(s);
    ready(w);
    const auto before = s.identifiedSnapshot();
    auto* d = open(w);
    require(list(*d)->count() == 0 && !apply(*d)->isEnabled(),
            "Unavailable support offered replacement");
    capture(*d, "unavailable-support");
    cancel(*d);
    require(s.identifiedSnapshot() == before, "Unavailable Cancel mutated state");
    std::cout << "unavailable support: no replacement, no mutation passed\n";
}
} // namespace
int main(int argc, char** argv) {
    QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication app(argc, argv);
    voelund::test::isolateSettings();
    try {
        require(argc == 2, "Artifact directory required");
        supportFlow(argv[1], false);
        supportFlow(argv[1], true);
        edgeAndStale(argv[1]);
        unavailable(argv[1]);
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
