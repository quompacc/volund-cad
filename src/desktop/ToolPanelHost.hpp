#pragma once

#include "FloatingCard.hpp"

#include <functional>
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

struct ToolLengthParameter final {
    std::string id;
    QString label;
    double value;
    double minimum;
    double maximum;
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
    // U09: Enter or focus loss ends the editing of a length input (one user action).
    std::function<void(const std::string&)> parameterFinished{};
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
    void setLengthEnabled(const std::string& parameterId, bool enabled);
    void setLengthVisible(const std::string& parameterId, bool visible);
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
};

} // namespace voelund::desktop
