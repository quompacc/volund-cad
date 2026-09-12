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

struct ToolPanelDefinition final {
    QString title;
    QString guidance;
    std::vector<ToolLengthParameter> lengthParameters;
    std::vector<ToolChoiceParameter> choiceParameters;
    std::function<void(const std::string&, double)> parameterChanged;
    std::function<void(const std::string&, int)> choiceChanged;
    std::function<void()> accept;
    std::function<void()> cancel;
};

class ToolPanelHost final : public QDockWidget {
public:
    explicit ToolPanelHost(QWidget* parent = nullptr);

    void begin(ToolPanelDefinition definition);
    void end();
    void setAcceptEnabled(bool enabled);
    void setLengthValue(const std::string& parameterId, double value);

private:
    void clearParameters();

    QWidget* content_{nullptr};
    QLabel* guidance_{nullptr};
    QFormLayout* parameterLayout_{nullptr};
    QPushButton* acceptButton_{nullptr};
    QPushButton* cancelButton_{nullptr};
    std::vector<std::pair<std::string, QDoubleSpinBox*>> lengthInputs_;
    std::vector<std::pair<std::string, QComboBox*>> choiceInputs_;
    ToolPanelDefinition definition_;
};

} // namespace voelund::desktop
