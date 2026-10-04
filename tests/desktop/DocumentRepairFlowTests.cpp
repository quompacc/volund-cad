#include "IsolatedSettings.hpp"
#include "../BoundSketchFixture.hpp"
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QTableWidget>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <voelund/desktop/MainWindow.hpp>
#include <voelund/document/DocumentSerializer.hpp>
using namespace voelund;
using namespace document;
namespace {
void require(bool ok, const char* message) {
    if (!ok)
        throw std::runtime_error(message);
}
void click(QWidget* w) {
    require(w && w->isEnabled(), "Missing/enabled button");
    const auto at = w->rect().center();
    QMouseEvent down(QEvent::MouseButtonPress, at, w->mapToGlobal(at), Qt::LeftButton,
                     Qt::LeftButton, Qt::NoModifier);
    QMouseEvent up(QEvent::MouseButtonRelease, at, w->mapToGlobal(at), Qt::LeftButton, Qt::NoButton,
                   Qt::NoModifier);
    QApplication::sendEvent(w, &down);
    QApplication::sendEvent(w, &up);
    QApplication::processEvents();
}
SketchParameters sketch(double z, double a = 3, double b = 4) {
    return {
        {0, 0, z, 0, 0, 1, 1, 0, 0}, {{{SketchElementId{1}, SketchRectangle{a, a, b, b}}}, 2}, {}};
}
ModelDocument fixture(bool ambiguous = false) {
    auto m = ModelDocument::empty();
    auto base = m.addBoxFeature("Support", {20, 20, 10});
    auto s = m.addSketchFeature("First", base, sketch(10));
    if (ambiguous) {
        auto p = sketch(10);
        p.elements.values.push_back({SketchElementId{2}, SketchRectangle{10, 10, 13, 13}});
        p.elements.nextId = 3;
        (void)m.setSketchParameters(s, p);
        test::bindTop(m, s);
    }
    auto pad = m.addExtrusionFeature("First pad", s, {2, {}});
    auto next = m.addSketchFeature("Dependent", pad, sketch(12, 3.2, 3.8));
    (void)m.addExtrusionFeature("Second pad", next, {1, {}});
    auto independent = m.addBoxFeature("Independent", {2, 2, 2});
    (void)m.addSketchFeature("Independent sketch", independent, sketch(2, .2, 1));
    return m;
}
void write(const ModelDocument& model, const std::filesystem::path& path) {
    std::ofstream f(path, std::ios::binary);
    DocumentSerializer::write(model, f);
}
std::vector<application::SketchSupportRepair> requests(const application::CadSession& s) {
    std::vector<application::SketchSupportRepair> r;
    for (const auto& p : s.suggestSketchSupportRepairs())
        if (p.proposal)
            r.push_back({p.inspection, *p.proposal});
    return r;
}
void core(const std::filesystem::path& dir) {
    const auto path = dir / "batch-source.volund";
    write(fixture(), path);
    application::CadSession s;
    s.loadDocument(path);
    const auto original = s.identifiedSnapshot();
    const auto stamp = s.sceneStatus().document;
    auto batch = requests(s);
    require(batch.size() == 3, "Dependent proposal missing");
    require(s.identifiedSnapshot() == original && s.sceneStatus().document == stamp &&
                !s.isModified() && !s.canUndo(),
            "Proposal mutates session");
    auto bad = batch;
    bad.back().chosen.binding.reference.body = BodyId{999};
    require(s.repairSketchSupports(bad).status() == application::CommandStatus::rejected &&
                s.identifiedSnapshot() == original && !s.canUndo(),
            "Batch partially committed");
    bad = batch;
    bad.push_back(bad.front());
    require(s.repairSketchSupports(bad).status() == application::CommandStatus::rejected,
            "Duplicate accepted");
    bad = batch;
    ++bad.back().expected.stamp.revision;
    require(s.repairSketchSupports(bad).status() == application::CommandStatus::rejected,
            "Stale accepted");
    bad = {batch[1]};
    require(s.repairSketchSupports(bad).status() == application::CommandStatus::rejected,
            "Unavailable upstream accepted");
    std::ranges::reverse(batch);
    require(s.repairSketchSupports(batch).status() == application::CommandStatus::committed,
            "Topological batch failed");
    const auto fixed = s.identifiedSnapshot();
    require(s.sceneStatus().isCurrent(), "Batch left errors");
    require(s.undo().status() == application::CommandStatus::committedWithErrors &&
                s.identifiedSnapshot() == original && !s.canUndo(),
            "Batch is not one undo");
    require(s.redo().status() == application::CommandStatus::committed &&
                s.identifiedSnapshot() == fixed,
            "Batch redo");
    s.saveDocument(dir / "batch-fixed.volund");
    s.loadDocument(dir / "batch-fixed.volund");
    require(s.identifiedSnapshot() == fixed, "Batch persistence");
    auto legacy = fixture();
    auto parameters = legacy.sketchParameters(FeatureId{2});
    parameters.elements.values.push_back({SketchElementId{2}, SketchLine{{3, 3}, {4, 4}}});
    parameters.elements.nextId = 3;
    parameters.constraints.values.push_back({SketchConstraintId{parameters.constraints.nextId++},
                                             SketchConstraintKind::coordinateX,
                                             {SketchElementId{2}, SketchPointPosition::start},
                                             std::nullopt,
                                             3,
                                             std::nullopt,
                                             {3, .5}});
    (void)legacy.setSketchParameters(FeatureId{2}, parameters);
    write(legacy, dir / "batch-legacy.volund");
    s.loadDocument(dir / "batch-legacy.volund");
    auto safe = requests(s);
    require(safe.size() == 1 && safe.front().expected.feature == FeatureId{7},
            "Auto proposed unclear legacy edges or blocked descendants");
    require(s.repairSketchSupports(safe).status() != application::CommandStatus::rejected &&
                !s.sketchParameters(FeatureId{7}).repairReason &&
                s.sketchParameters(FeatureId{2}).repairReason,
            "Unclear edges blocked independent automatic repair");
    std::cout << "batch chain: read-only planning, atomic rejection, ordering, Undo/Redo and "
                 "persistence passed\n";
}
QDialog* open(desktop::MainWindow& w) {
    auto* a = w.findChild<QAction*>("repairAllReferencesAction");
    require(a && a->isEnabled(), "Document action unavailable");
    a->trigger();
    QApplication::processEvents();
    for (auto* d : w.findChildren<QDialog*>())
        if (d->isVisible() && d->objectName() == "documentRepairDialog")
            return d;
    throw std::runtime_error("No document dialog");
}
void capture(QWidget& d, const QString& name) {
    const auto dir = qEnvironmentVariable("VOELUND_BATCH_REPAIR_ARTIFACT_DIR");
    if (dir.isEmpty())
        return;
    require(QDir().mkpath(dir), "Capture directory");
    QApplication::processEvents();
    require(d.grab().save(dir + "/" + name + ".png"), "Capture");
}
void ui(const std::filesystem::path& dir, bool ambiguous) {
    const auto path = dir / (ambiguous ? "batch-ambiguous.volund" : "batch-ui-source.volund");
    write(fixture(ambiguous), path);
    application::CadSession s;
    s.loadDocument(path);
    require(s.selectFeature(FeatureId{1}), "Select box");
    desktop::MainWindow w(s);
    w.resize(1400, 900);
    w.show();
    QApplication::processEvents();
    const auto original = s.identifiedSnapshot();
    auto* d = open(w);
    auto* table = d->findChild<QTableWidget*>("repairBatchTable");
    require(table->rowCount() == (ambiguous ? 2 : 3), "Wrong review count");
    for (int i = 0; i < table->rowCount(); ++i)
        require(table->item(i, 0)->checkState() == Qt::Unchecked, "Opening auto-applied");
    if (!ambiguous) {
        click(d->findChild<QPushButton*>("repairBatchSelect"));
        for (int i = 0; i < table->rowCount(); ++i)
            require(table->item(i, 0)->checkState() == Qt::Checked, "Multi selection missing");
        capture(*d, "batch-selected");
        click(d->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Close));
        require(s.identifiedSnapshot() == original && !s.canUndo(), "Cancel changed model");
        d = open(w);
        click(d->findChild<QPushButton*>("repairBatchSelect"));
        click(d->findChild<QPushButton*>("repairBatchApply"));
        require(s.sceneStatus().isCurrent(), "Apply checked failed");
        require(s.undo().status() == application::CommandStatus::committedWithErrors &&
                    s.identifiedSnapshot() == original,
                "Multi Undo");
        click(d->findChild<QPushButton*>("repairBatchRefresh"));
        d->resize(780, 500);
        QApplication::processEvents();
        capture(*d, "batch-narrow");
    }
    click(d->findChild<QPushButton*>("repairAuto"));
    table = d->findChild<QTableWidget*>("repairBatchTable");
    if (ambiguous) {
        require(table->rowCount() == 1 && s.sketchParameters(FeatureId{4}).repairReason &&
                    !s.sketchParameters(FeatureId{7}).repairReason,
                "Auto chose ambiguous reference or missed unique one");
        capture(*d, "batch-ambiguity-remains");
    } else {
        require(table->rowCount() == 0 && s.sceneStatus().isCurrent(),
                "Auto requires another confirmation");
        capture(*d, "batch-auto-complete");
    }
    require(s.undo().status() == application::CommandStatus::committedWithErrors &&
                s.identifiedSnapshot() == original && !s.canUndo(),
            "Auto Undo not atomic");
    click(d->findChild<QPushButton*>("repairBatchRefresh"));
    click(d->findChild<QPushButton*>("repairBatchSelect"));
    s.duplicateDocument();
    const auto replaced = s.identifiedSnapshot();
    click(d->findChild<QPushButton*>("repairBatchApply"));
    require(s.identifiedSnapshot() == replaced && !s.canUndo() &&
                d->findChild<QLabel*>("repairBatchMessage")->text().contains("stale"),
            "Stale batch committed");
    capture(*d, ambiguous ? "batch-stale-ambiguous" : "batch-stale");
    click(d->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Close));
    std::cout << (ambiguous ? "ambiguous review retained" : "multi selection and immediate Auto")
              << ": UI and atomic Undo passed\n";
}
} // namespace
int main(int argc, char** argv) {
    QApplication app(argc, argv);
    voelund::test::isolateSettings();
    try {
        require(argc == 2, "Fixture directory");
        core(argv[1]);
        ui(argv[1], false);
        ui(argv[1], true);
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
