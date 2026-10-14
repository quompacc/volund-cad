#include "SketchToolActions.hpp"
#include "MainWindowSupport.hpp"
#include "ToolPanelHost.hpp"

#include <QAction>
#include <QActionGroup>
#include <QKeySequence>
#include <QWidget>

#include <algorithm>
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
        {"center_rectangle", QStringLiteral("Shift+R")},
        {"point", QStringLiteral("O")}};
    const auto match = keys.find(id);
    return match == keys.end() ? QString{} : match->second;
}

// Sections and families of the rail, by command ID; the first entry of a family is its
// button until another variant is used.
struct RailSection {
    const char* title;
    std::vector<std::vector<const char*>> families;
};
const std::vector<RailSection>& railSections() {
    static const std::vector<RailSection> sections{
        {QT_TR_NOOP("Zeichnen"),
         {{"sketch.tool.select"},
          {"sketch.tool.line", "sketch.tool.polyline"},
          {"sketch.tool.rectangle", "sketch.tool.center_rectangle"},
          {"sketch.tool.circle", "sketch.tool.ellipse"},
          {"sketch.tool.center_arc", "sketch.tool.three_point_arc"},
          {"sketch.tool.slot"},
          {"sketch.tool.polygon"},
          {"sketch.tool.point"}}},
        {QT_TR_NOOP("Beziehungen"),
         {{"sketch.horizontal", "sketch.vertical", "sketch.free"},
          {"sketch.parallel", "sketch.perpendicular", "sketch.collinear"},
          {"sketch.coincident", "sketch.midpoint", "sketch.point_on_object",
           "sketch.horizontal_points", "sketch.vertical_points", "sketch.symmetric",
           "sketch.perpendicular_bisector"},
          {"sketch.fixed", "sketch.equal", "sketch.tangent", "sketch.concentric"}}},
        {QT_TR_NOOP("Bemaßungen"),
         {{"sketch.point_distance", "sketch.point_distance_x", "sketch.point_distance_y",
           "sketch.point_line_distance"},
          {"sketch.diameter", "sketch.radius"},
          {"sketch.angle"}}},
        {QT_TR_NOOP("Bearbeiten"),
         {{"sketch.trim", "sketch.extend", "sketch.split"},
          {"sketch.fillet"},
          {"sketch.offset"},
          {"sketch.mirror", "sketch.linear_pattern", "sketch.circular_pattern",
           "sketch.edit_pattern", "sketch.dissolve_pattern"},
          {"sketch.lock_point", "sketch.unlock_point"},
          {"sketch.construction"},
          {"sketch.delete", "sketch.remove_constraint"}}}};
    return sections;
}
const char* const withoutRailPlace[]{"sketch.undo_sketch", "sketch.redo_sketch"};

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

std::vector<SketchToolActions::Section> SketchToolActions::sections() const {
    std::map<QString, QAction*> byId;
    for (const Group& group : groups_)
        for (QAction* action : group.actions)
            byId.emplace(action->property(commandIdProperty).toString(), action);
    std::vector<Section> result;
    for (const RailSection& section : railSections()) {
        Section placed{QObject::tr(section.title), {}};
        for (const auto& family : section.families) {
            std::vector<QAction*> actions;
            for (const char* id : family)
                if (const auto found = byId.find(QString::fromLatin1(id)); found != byId.end()) {
                    actions.push_back(found->second);
                    byId.erase(found);
                }
            if (!actions.empty())
                placed.families.push_back(std::move(actions));
        }
        result.push_back(std::move(placed));
    }
    for (const char* id : withoutRailPlace)
        byId.erase(QString::fromLatin1(id));
    // A command the table does not know yet still gets a button, at the end of its group.
    for (const Group& group : groups_)
        for (QAction* action : group.actions)
            if (byId.contains(action->property(commandIdProperty).toString())) {
                auto section = std::ranges::find(result, group.title, &Section::title);
                if (section == result.end())
                    section = result.insert(result.end(), Section{group.title, {}});
                section->families.push_back({action});
            }
    return result;
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
