#include "CadSessionTestSupport.hpp"

#include <voelund/application/Actions.hpp>

#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

// N01b (user decision 2026-10-21): a pattern created with its count bound to N and its spacing to
// S (pending bindings, one undo step); N = 4 makes four cubes, 2.5 and 1 are refused. The cube's
// width bound to W follows W = 20; undo returns. N cannot be deleted while bound; unbinding keeps
// the count. Save/open keeps the bindings; deleting the pattern takes its bindings along; an
// unknown field is refused and changes nothing; pending with a feature command it is left out.
// A number typed into a bound field unbinds it.
namespace {
using namespace voelund;
using application::CommandStatus;
using document::ParameterUnit;

void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}
bool near(double a, double b) { return std::abs(a - b) < 1e-6 * std::max(1.0, std::abs(b)); }
double volume(const application::CadSession& session) {
    double total = 0;
    for (const auto& body : session.validBodies())
        total += test::volumeOf(body.shape);
    return total;
}
document::ParameterId named(const application::CadSession& session, const std::string& name) {
    for (const auto& entry : session.parameterTable().values)
        if (entry.name == name)
            return entry.id;
    throw std::runtime_error("Parameter " + name);
}

struct Model final {
    application::CadSession session;
    document::FeatureId box{0};
    document::FeatureId pattern{0};
};

void patternBound(Model& model) {
    auto& session = model.session;
    session.newDocument();
    model.box = *session.createBoxFeature({10, 10, 10}).feature();
    for (const auto& [name, unit, value] :
         {std::tuple{"N", ParameterUnit::number, 2.0}, std::tuple{"S", ParameterUnit::length, 20.0},
          std::tuple{"W", ParameterUnit::length, 10.0}})
        require(session.createParameter(name, unit, value).status() == CommandStatus::committed,
                std::string("Parameter ") + name);
    application::PatternInput input;
    input.baseAxis = document::BaseAxis::x;
    input.count = 3;
    input.spacing = 15;
    session.pendingFieldBindings() = {{std::nullopt, "count", named(session, "N")},
                                      {std::nullopt, "spacing", named(session, "S")}};
    const auto created =
        application::executeAction(session,
                                   application::prepareRequest(
                                       session, application::CreatePatternAction{model.box, input}))
            .command;
    require(created.status() == CommandStatus::committed && created.feature(),
            "Pattern created: " + created.diagnostic());
    model.pattern = *created.feature();
    const auto pattern = session.patternParameters(model.pattern);
    require(pattern.count == 2 && near(pattern.spacing, 20) &&
                session.parameterTable().bindings.size() == 2 && session.validBodies().size() == 2,
            "The new pattern takes N = 2 and S = 20 at once");
    require(session.undo().status() == CommandStatus::committed &&
                session.parameterTable().bindings.empty() && session.validBodies().size() == 1,
            "One undo removes the pattern and its bindings");
    require(session.redo().status() == CommandStatus::committed &&
                session.parameterTable().bindings.size() == 2,
            "Redo brings both back");
}

void valuesFollow(Model& model) {
    auto& session = model.session;
    require(session.setParameterValue(named(session, "N"), 4).status() ==
                    CommandStatus::committed &&
                session.validBodies().size() == 4 && near(volume(session), 4000),
            "N = 4: four cubes");
    for (const double wrong : {2.5, 1.0})
        require(session.setParameterValue(named(session, "N"), wrong).status() ==
                        CommandStatus::rejected &&
                    session.patternParameters(model.pattern).count == 4,
                "A count of " + std::to_string(wrong) + " is refused");
    require(session.setParameterValue(named(session, "W"), 12).status() ==
                    CommandStatus::committed &&
                near(volume(session), 4000),
            "W alone is not bound yet");
    require(session.bindFeatureField(model.box, "width", named(session, "W")).status() ==
                    CommandStatus::committed &&
                near(volume(session), 4 * 1200),
            "Bound to W = 12 the cubes are 12 wide: " + std::to_string(volume(session)));
    require(
        session.setParameterValue(named(session, "W"), 20).status() == CommandStatus::committed &&
            near(volume(session), 4 * 2000) && session.parameterUseCount(named(session, "W")) == 1,
        "W = 20 follows");
    require(session.undo().status() == CommandStatus::committed && near(volume(session), 4 * 1200),
            "Undo returns to 12");
}

void unbindAndDelete(Model& model) {
    auto& session = model.session;
    const auto count = named(session, "N");
    require(session.deleteParameter(count).status() == CommandStatus::rejected,
            "N cannot go while the count is bound");
    require(session.deleteParameter(count, application::ParameterDeletion::unbindUses).status() ==
                    CommandStatus::committed &&
                session.patternParameters(model.pattern).count == 4 &&
                session.parameterTable().bindings.size() == 2,
            "Unbinding keeps the count of 4");
    session.pendingFieldBindings() = {{model.pattern, "angle", named(session, "S")}};
    auto changed = session.patternParameters(model.pattern);
    changed.reversed = true;
    const auto edited = session.editPatternParameters(model.pattern, changed);
    require(edited.status() == CommandStatus::committed &&
                session.parameterTable().bindings.size() == 2 &&
                session.pendingFieldBindings().empty(),
            "A pending field the linear pattern lacks is left out: " + edited.diagnostic());
    // A number typed into a bound field unbinds it and stays.
    changed = session.patternParameters(model.pattern);
    changed.spacing = 30;
    require(session.editPatternParameters(model.pattern, changed).status() ==
                    CommandStatus::committed &&
                near(session.patternParameters(model.pattern).spacing, 30) &&
                session.parameterTable().bindings.size() == 1,
            "Spacing 30 typed: unbound from S, the 30 stays");
    require(session.undo().status() == CommandStatus::committed &&
                near(session.patternParameters(model.pattern).spacing, 20) &&
                session.parameterTable().bindings.size() == 2,
            "Undo binds it to S = 20 again");
}

void savedAndDeleted(Model& model) {
    auto& session = model.session;
    const auto path = std::filesystem::temp_directory_path() / "voelund-field-binding-Ä.volund";
    session.saveDocument(path);
    application::CadSession reopened;
    reopened.loadDocument(path);
    std::filesystem::remove(path);
    require(reopened.parameterTable().bindings == session.parameterTable().bindings &&
                near(volume(reopened), volume(session)),
            "Save/open keeps the bindings");
    const std::vector<document::FeatureId> pattern{model.pattern};
    require(session.deleteFeatures(pattern).status() == CommandStatus::committed &&
                session.parameterTable().bindings.size() == 1 &&
                session.parameterTable().bindings.front().feature == model.box,
            "The pattern's bindings go with it");
    const auto revision = session.sceneStatus().document;
    const auto unknown = session.bindFeatureField(model.box, "colour", named(session, "S"));
    require(unknown.status() == CommandStatus::rejected &&
                session.sceneStatus().document == revision,
            "An unknown field is refused: " + unknown.diagnostic());
    const auto unbound = session.bindFeatureField(model.box, "width", std::nullopt);
    require(unbound.status() == CommandStatus::committed &&
                session.parameterTable().bindings.empty() && near(volume(session), 1200),
            "Unbound, the width keeps 12");
}

} // namespace

int main() {
    try {
        Model model;
        patternBound(model);
        valuesFollow(model);
        unbindAndDelete(model);
        savedAndDeleted(model);
        std::cout << "PASS N01b field bindings in the session: pending bindings with a new "
                     "pattern in one undo step, count and width follow, wrong counts refused, "
                     "delete/unbind, save/open, bindings go with their feature\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
