#pragma once

#include <QDockWidget>

#include <functional>
#include <string>
#include <vector>

class QDoubleSpinBox;
class QComboBox;
class QFormLayout;
class QLabel;
class QPushButton;
class QToolBox;
class QShortcut;

namespace voelund::desktop {

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
};

class ToolPanelHost final : public QDockWidget {
  public:
    explicit ToolPanelHost(QWidget* parent = nullptr);

    void begin(ToolPanelDefinition definition);
    void end();
    void setAcceptEnabled(bool enabled);
    void setGuidance(const QString& guidance);
    void setLengthValue(const std::string& parameterId, double value);
    void setLengthEnabled(const std::string& parameterId, bool enabled);
    void setActionEnabled(const std::string& actionId, bool enabled);

  private:
    void clearParameters();

    QWidget* content_{nullptr};
    QLabel* guidance_{nullptr};
    QFormLayout* parameterLayout_{nullptr};
    QToolBox* actionToolBox_{nullptr};
    QPushButton* acceptButton_{nullptr};
    QPushButton* cancelButton_{nullptr};
    QShortcut* escapeShortcut_{nullptr};
    QShortcut* undoShortcut_{nullptr};
    QShortcut* redoShortcut_{nullptr};
    std::vector<std::pair<std::string, QDoubleSpinBox*>> lengthInputs_;
    std::vector<std::pair<std::string, QComboBox*>> choiceInputs_;
    std::vector<std::pair<std::string, QPushButton*>> actionInputs_;
    ToolPanelDefinition definition_;
};

} // namespace voelund::desktop
