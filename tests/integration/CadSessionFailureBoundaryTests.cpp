#include "../../src/features/FeatureEvaluationDetail.hpp"
#include <Standard_OutOfMemory.hxx>
#include <Standard_ProgramError.hxx>
#include <iostream>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <voelund/application/CadSession.hpp>
#include <voelund/kernel/OcctCompatibility.hpp>

namespace {
using namespace voelund;
using application::CommandStatus;
using application::ErrorDisposition;
void require(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
struct Hook {
    decltype(features::detail::beforeFeatureEvaluation) previous;
    explicit Hook(decltype(previous) hook) : previous(features::detail::beforeFeatureEvaluation) {
        features::detail::beforeFeatureEvaluation = hook;
    }
    ~Hook() { features::detail::beforeFeatureEvaluation = previous; }
};
template <class Error> void inject(document::FeatureId id) {
    if (id != document::ModelDocument::initialCylinderId())
        return;
    if constexpr (std::is_same_v<Error, std::bad_alloc>)
        throw Error{};
    else
        throw Error("injected evaluation failure");
}
void kernelFailure(document::FeatureId id) {
    if (id == document::ModelDocument::initialCylinderId())
        throw kernel::KernelError(kernel::KernelOperation::fuse, "injected geometry failure");
}
template <class Error, class Operation> void expectException(Operation operation) {
    bool caught = false;
    try {
        Hook hook(inject<Error>);
        operation();
    } catch (const Error&) {
        caught = true;
    }
    require(caught, "infrastructure error was converted into an editable geometry error");
}
template <class Error> void rollback() {
    application::CadSession session;
    static_cast<void>(session.createBoxFeature());
    require((session.undo().status() == voelund::application::CommandStatus::committed) &&
                !session.isModified() &&
                session.selectFeature(document::ModelDocument::initialBoxId()),
            "redo fixture");
    const auto shape = session.currentShape();
    const auto selection = session.selectedFeature();
    const auto original = session.selectedBoxParameters();
    auto parameters = original;
    parameters.dimensions.width += 2;
    expectException<Error>([&] {
        static_cast<void>(
            session.editSelectedBoxParameters(parameters, ErrorDisposition::keepEditableErrors));
    });
    require(!session.isModified() && session.canRedo() && !session.canUndo() &&
                session.selectedFeature() == selection && session.currentShape().IsSame(shape) &&
                session.selectedBoxParameters() == original,
            "failed parameter evaluation published state");
    expectException<Error>([&] { static_cast<void>(session.createBoxFeature()); });
    require(!session.isModified() && session.canRedo() && session.features().size() == 2,
            "failed creation published state");
    require(session.editSelectedBoxParameters(parameters).status() == CommandStatus::committed,
            "valid edit after failure");
    const auto changedShape = session.currentShape();
    expectException<Error>([&] {
        static_cast<void>(
            (session.undo().status() == voelund::application::CommandStatus::committed));
    });
    require(session.selectedBoxParameters() == parameters && session.canUndo() &&
                !session.canRedo() && session.currentShape().IsSame(changedShape),
            "failed undo partially published history");
    require((session.undo().status() == voelund::application::CommandStatus::committed) &&
                !session.isModified(),
            "undo recovery");
    expectException<Error>([&] {
        static_cast<void>(
            (session.redo().status() == voelund::application::CommandStatus::committed));
    });
    require(!session.isModified() && !session.canUndo() && session.canRedo() &&
                session.selectedBoxParameters() == original,
            "failed redo partially published history");
    require((session.redo().status() == voelund::application::CommandStatus::committed) &&
                session.selectedBoxParameters() == parameters,
            "redo recovery");
}
void expectedFailurePolicy() {
    application::CadSession session;
    const auto original = session.selectedBoxParameters();
    auto parameters = original;
    parameters.dimensions.width += 3;
    {
        Hook hook(kernelFailure);
        const auto rejected = session.editSelectedBoxParameters(parameters);
        require(rejected.status() == CommandStatus::rejected && !session.isModified() &&
                    rejected.reason() == application::RejectionReason::evaluationFailed,
                "default parameter policy retained an error");
        const auto kept =
            session.editSelectedBoxParameters(parameters, ErrorDisposition::keepEditableErrors);
        require(kept.status() == CommandStatus::committedWithErrors && session.isModified(),
                "explicit error policy not reflected in result");
    }
    require((session.undo().status() == voelund::application::CommandStatus::committed) &&
                session.selectedBoxParameters() == original && !session.isModified(),
            "explicit error commit must have one undo");
    require(session.editSelectedBoxParameters(original).reason() ==
                    application::RejectionReason::noChange &&
                session.canRedo(),
            "unchanged setter consumed redo");
}
} // namespace
int main() {
    try {
        rollback<std::bad_alloc>();
        rollback<std::logic_error>();
        rollback<std::runtime_error>();
        rollback<Standard_OutOfMemory>();
        rollback<Standard_ProgramError>();
        expectedFailurePolicy();
        std::cout << "Typed edits and exception rollback tests passed\n";
        return 0;
    } catch (const Standard_Failure& error) {
        std::cerr << kernel::occtFailureDiagnostic(error) << '\n';
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
    }
    return 1;
}
