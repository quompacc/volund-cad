#include "SketchToolActions.hpp"
#include "MainWindowSupport.hpp"
#include "ToolPanelHost.hpp"

#include <QAction>
#include <QActionGroup>
#include <QKeySequence>
#include <QWidget>

#include <map>
#include <string>

namespace voelund::desktop {
namespace {

const ToolChoiceParameter* toolChoice(const ToolPanelDefinition& definition) {
    for (const ToolChoiceParameter& choice : definition.choiceParameters)
        if (choice.id == "tool" && !choice.choiceIds.empty())
            return &choice;
    return nullptr;
}

QString toolShortcut(const std::string& id) {
    static const std::map<std::string, QString> keys{
        {"select", QStringLiteral("S")},
        {"line", QStringLiteral("L")},
        {"polyline", QStringLiteral("P")},
        {"rectangle", QStringLiteral("R")},
        {"circle", QStringLiteral("C")},
        {"center_arc", QStringLiteral("A")},
        {"three_point_arc", QStringLiteral("T")},
        {"ellipse", QStringLiteral("E")},
        {"slot", QStringLiteral("N")},
        {"polygon", QStringLiteral("G")},
        {"center_rectangle", QStringLiteral("Shift+R")}};
    const auto match = keys.find(id);
    return match == keys.end() ? QString{} : match->second;
}

} // namespace

SketchToolActions::SketchToolActions(QWidget& window, ToolPanelHost& host)
    : window_(window), host_(host) {}

bool SketchToolActions::sketchActive() const {
    return host_.isActive() && toolChoice(host_.definition()) != nullptr;
}

void SketchToolActions::build() {
    const ToolPanelDefinition& definition = host_.definition();
    const ToolChoiceParameter* choice = toolChoice(definition);
    tools_ = new QActionGroup(&window_);
    tools_->setExclusive(true);
    Group drawing{QObject::tr("Zeichnen"), {}};
    for (std::size_t index = 0; index < choice->choiceIds.size(); ++index) {
        const std::string& id = choice->choiceIds[index];
        auto* action = new QAction(choice->choices[index], &window_);
        action->setCheckable(true);
        action->setActionGroup(tools_);
        action->setShortcut(QKeySequence(toolShortcut(id)));
        action->setToolTip(QStringLiteral("%1  (%2)").arg(choice->choices[index],
                                                          toolShortcut(id)));
        setCommandId(*action, ("sketch.tool." + id).c_str());
        const int choiceIndex = static_cast<int>(index);
        QObject::connect(action, &QAction::triggered, &window_,
                         [this, choiceIndex] { host_.chooseOption("tool", choiceIndex); });
        window_.addAction(action);
        drawing.actions.push_back(action);
    }
    groups_.push_back(std::move(drawing));
    for (const ToolAction& tool : definition.actions) {
        auto group = groups_.begin();
        while (group != groups_.end() && group->title != tool.group)
            ++group;
        if (group == groups_.end())
            group = groups_.insert(groups_.end(), Group{tool.group, {}});
        auto* action = new QAction(compactActionLabel(tool.id, tool.label), &window_);
        action->setToolTip(tool.label);
        setCommandId(*action, ("sketch." + tool.id).c_str());
        const std::string id = tool.id;
        QObject::connect(action, &QAction::triggered, &window_,
                         [this, id] { host_.triggerAction(id); });
        window_.addAction(action);
        group->actions.push_back(action);
    }
}

void SketchToolActions::sync() {
    const bool active = sketchActive();
    if (active && groups_.empty())
        build();
    if (groups_.empty())
        return;
    const int current = active ? host_.choiceIndex("tool") : -1;
    const auto& tools = groups_.front().actions;
    for (std::size_t index = 0; index < tools.size(); ++index) {
        tools[index]->setEnabled(active);
        tools[index]->setChecked(static_cast<int>(index) == current);
    }
    for (std::size_t group = 1; group < groups_.size(); ++group)
        for (QAction* action : groups_[group].actions) {
            const std::string id = action->property(commandIdProperty)
                                       .toString()
                                       .mid(QStringLiteral("sketch.").size())
                                       .toStdString();
            action->setEnabled(active && host_.isActionEnabled(id));
        }
}

} // namespace voelund::desktop
