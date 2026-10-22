// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include "FloatingCard.hpp"

#include <voelund/application/FieldBindingRequest.hpp>
#include <voelund/document/DocumentParameters.hpp>

#include <QString>
#include <QStringList>

#include <functional>
#include <optional>
#include <string>
#include <vector>

class QDoubleSpinBox;
class QButtonGroup;
class QComboBox;
class QFormLayout;
class QLabel;
class QPushButton;
class QTabWidget;
class QShortcut;

namespace voelund::desktop {

// Short button label for a tool action (panel and tool rail, U13).
[[nodiscard]] QString compactActionLabel(const std::string& id, const QString& fallback);

// U11 (A-VIS-01): what a numeric field holds, so its unit is right.
enum class ToolValueKind { length, count, angle, factor }; // factor: S20, no unit

struct ToolLengthParameter final {
    std::string id;
    QString label;
    double value;
    double minimum;
    double maximum;
    ToolValueKind kind{ToolValueKind::length};
    // PAR02: the field also takes "=Name" of a document parameter (N01b: counts too);
    // `boundName` shows an existing binding.
    bool bindable{false};
    std::optional<QString> boundName{};
};

// PAR02: a parameter named in a field, found or created on request.
struct ResolvedParameter final {
    document::ParameterId id;
    bool created{false}; // created now: only the parameter table changed
};

struct ToolChoiceParameter final {
    std::string id;
    QString label;
    std::vector<QString> choices;
    int currentIndex;
    // Stable IDs of the choices for command IDs (U13); empty when not offered elsewhere.
    std::vector<std::string> choiceIds{};
};

struct ToolAction final {
    std::string id;
    QString label;
    QString group;
};

struct ToolPanelDefinition final {
    QString title;
    QString guidance;
    std::vector<ToolLengthParameter> lengthParameters;
    std::vector<ToolChoiceParameter> choiceParameters;
    std::vector<ToolAction> actions;
    std::function<void(const std::string&, double)> parameterChanged;
    std::function<void(const std::string&, int)> choiceChanged;
    std::function<void(const std::string&)> actionTriggered;
    std::function<void()> accept;
    std::function<void()> cancel;
    std::function<void()> stepBack;
    // S13: when set, Enter calls this instead of OK (the sketch editor ends a polyline);
    // Ctrl+Enter confirms like OK in every tool.
    std::function<void()> returnPressed{};
    // U09: Enter or focus loss ends the editing of a length input (one user action).
    std::function<void(const std::string&)> parameterFinished{};
    // PAR02: a bindable field was bound to a parameter, or (nullopt) unbound by a number.
    std::function<void(const std::string&, std::optional<ResolvedParameter>)> parameterBound{};
    // N01b: without `parameterBound`, bindable fields bind through the document's field list,
    // named by their IDs; `editedFeature` is the feature the tool changes (none: it creates
    // one). A parameter created on the way changed only the table: `parameterTableChanged`.
    std::optional<document::FeatureId> editedFeature{};
    std::function<void()> parameterTableChanged{};
};

// N01b: the definition of a feature tool whose number fields take "=Name": they bind through
// the document's field list to `edited` (none: the feature the tool creates); `tableChanged`
// runs when a parameter was created on the way.
[[nodiscard]] ToolPanelDefinition withFieldBindings(ToolPanelDefinition definition,
                                                    std::optional<document::FeatureId> edited,
                                                    std::function<void()> tableChanged);

// N01b: what the shell tells the panel about field bindings, and is told by it.
struct FieldBindingHooks final {
    // The parameter a field of a feature is bound to.
    std::function<std::optional<QString>(document::FeatureId, const std::string&)> boundName;
    // Name and value of a parameter.
    std::function<std::optional<std::pair<QString, double>>(document::ParameterId)> parameter;
    // The bindings asked for in this tool session (empty when it ends).
    std::function<void(const std::vector<application::FieldBindingRequest>&)> requested;
    // OK ended the tool session with these bindings; those its command did not take yet (e.g.
    // when only a binding changed) are the shell's to make.
    std::function<void(const std::vector<application::FieldBindingRequest>&)> accepted;
};

// U05: the floating tool panel of ADR 0012; it owns at most one active tool session.
class ToolPanelHost final : public FloatingCard {
  public:
    explicit ToolPanelHost(QWidget* host);

    void begin(ToolPanelDefinition definition);
    void end();
    // Notified when a tool session starts or ends (U04: the shell hides its context bar).
    void setActivityHandler(std::function<void()> handler);
    void setAcceptEnabled(bool enabled);
    void setGuidance(const QString& guidance);
    // A fixed line above the guidance, e.g. the sketch's degrees of freedom; empty hides it.
    void setStatus(const QString& status, bool good = false);
    void setErrorCommitHandler(std::function<void()> handler);
    void setLengthValue(const std::string& parameterId, double value);
    // PAR02: shows a bindable field as bound to `name`, or (nullopt) as a plain number.
    void setLengthBinding(const std::string& parameterId, std::optional<QString> name);
    // PAR02: finds the parameter of that name and unit or offers to create it with `value`;
    // nullopt when there is none, the unit differs or the user declined (set by the shell).
    using ParameterResolver = std::function<std::optional<ResolvedParameter>(
        const QString& name, ToolValueKind kind, double value)>;
    void setParameterResolver(ParameterResolver resolver);
    void setParameterNames(std::function<QStringList(ToolValueKind)> names);
    void setFieldBindingHooks(FieldBindingHooks hooks);
    // N01b: binds a field through the document's field list (nullopt: unbinds it), for tools
    // that bind some fields themselves.
    void bindThroughDocument(const std::string& id, std::optional<ResolvedParameter> resolved);
    [[nodiscard]] std::optional<ResolvedParameter>
    resolveParameter(const QString& name, ToolValueKind kind, double value) const;
    void setLengthEnabled(const std::string& parameterId, bool enabled);
    void setLengthVisible(const std::string& parameterId, bool visible);
    // M04: choices that do not apply are disabled or hidden, never left misleading.
    void setChoiceEnabled(const std::string& choiceId, bool enabled);
    void setChoiceVisible(const std::string& choiceId, bool visible);
    void setActionEnabled(const std::string& actionId, bool enabled);

    // U13: the tool rail offers the session's tool choice and actions; the panel then keeps
    // only values, feedback and OK/Cancel.
    void setToolsInRail(bool inRail);
    void setStateHandler(std::function<void()> handler);
    [[nodiscard]] bool isActive() const noexcept { return active_; }
    [[nodiscard]] const ToolPanelDefinition& definition() const noexcept { return definition_; }
    [[nodiscard]] bool isActionEnabled(const std::string& actionId) const;
    [[nodiscard]] int choiceIndex(const std::string& choiceId) const;
    void chooseOption(const std::string& choiceId, int index);
    void triggerAction(const std::string& actionId);

  private:
    void clearParameters();
    void syncParameterGroup();
    void addLengthInput(const ToolLengthParameter& parameter);
    void bindField(const std::string& id, ToolValueKind kind, std::optional<QString> name);
    void requestFields();
    void showBinding(const std::string& id, std::optional<document::ParameterId> bound);
    void acceptPressed();

    QWidget* content_{nullptr};
    QLabel* guidance_{nullptr};
    QLabel* status_{nullptr};
    QWidget* parameterContainer_{nullptr};
    QFormLayout* parameterLayout_{nullptr};
    QTabWidget* actionTabs_{nullptr};
    QPushButton* acceptButton_{nullptr};
    QPushButton* cancelButton_{nullptr};
    QPushButton* errorCommitButton_{nullptr};
    std::function<void()> errorCommitHandler_;
    std::function<void()> activityHandler_;
    std::function<void()> stateHandler_;
    std::vector<std::pair<std::string, int>> choiceIndices_;
    QButtonGroup* toolGroup_{nullptr};
    QWidget* toolChoices_{nullptr};
    bool toolsInRail_{false};
    bool active_{false};
    QShortcut* escapeShortcut_{nullptr};
    QShortcut* undoShortcut_{nullptr};
    QShortcut* redoShortcut_{nullptr};
    QShortcut* acceptShortcut_{nullptr};
    std::vector<std::pair<std::string, QDoubleSpinBox*>> lengthInputs_;
    std::vector<std::pair<std::string, QComboBox*>> choiceInputs_;
    std::vector<std::pair<std::string, QPushButton*>> actionInputs_;
    ToolPanelDefinition definition_;
    ParameterResolver resolver_;
    std::function<QStringList(ToolValueKind)> names_;
    FieldBindingHooks fieldHooks_;
    std::vector<application::FieldBindingRequest> fieldRequests_;
};

} // namespace voelund::desktop
