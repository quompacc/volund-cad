#include "../../apps/voelund-ui-preview/SketchBackend.hpp"
#include "../../src/features/FeatureEvaluationDetail.hpp"
#include <QApplication>
#include <QPushButton>
#include <Standard_OutOfMemory.hxx>
#include <Standard_ProgramError.hxx>
#include <iostream>
#include <voelund/application/CadSession.hpp>

namespace {
using namespace voelund;
void require(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
template <class Error>
void rejectedException(application::CadSession& session, preview::SketchBackend& backend,
                       QPushButton& button) {
    const auto stamp = session.sceneStatus();
    const auto shape = session.currentShape();
    const auto selection = session.selectedFeature();
    const auto size = session.features().size();
    features::detail::beforeFeatureEvaluation = [](document::FeatureId) {
        if constexpr (std::is_same_v<Error, std::bad_alloc>)
            throw std::bad_alloc{};
        else
            throw Error("injected preview exception");
    };
    button.click();
    features::detail::beforeFeatureEvaluation = nullptr;
    require(backend.active() && backend.lastMessage().contains("Operation failed") &&
                session.sceneStatus().document == stamp.document &&
                session.currentShape().IsSame(shape) && session.selectedFeature() == selection &&
                session.features().size() == size && !session.isModified() && !session.canUndo(),
            "Preview exception changed or closed the sketch");
}
void run() {
    application::CadSession session;
    preview::SketchBackend backend(session);
    QPushButton finish("Finish sketch");
    bool completed = false;
    QObject::connect(&finish, &QPushButton::clicked, [&] { completed = backend.finish(); });
    require(backend.begin(), "preview begin");
    finish.click();
    require(!completed && backend.active(), "empty preview was closed");
    require(backend.add(preview::PreviewCanvas::SketchTool::Rectangle, {1, 1}, {5, 4}),
            "preview rectangle");
    rejectedException<std::bad_alloc>(session, backend, finish);
    rejectedException<std::logic_error>(session, backend, finish);
    rejectedException<Standard_ProgramError>(session, backend, finish);
    rejectedException<Standard_OutOfMemory>(session, backend, finish);
    finish.click();
    require(completed && !backend.active() && session.features().size() == 3 &&
                session.undo().status() == application::CommandStatus::committed &&
                !session.isModified(),
            "Preview recovery did not commit exactly one action");
}
} // namespace
int main(int argc, char** argv) {
    QApplication app(argc, argv);
    try {
        run();
        std::cout << "Preview command exceptions and recovery passed\n";
        return 0;
    } catch (const std::exception& error) {
        features::detail::beforeFeatureEvaluation = nullptr;
        std::cerr << error.what() << '\n';
        return 1;
    }
}
