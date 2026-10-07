#pragma once

#include <QFrame>
#include <QIcon>
#include <QString>

#include <functional>

class QLabel;
class QPushButton;

namespace voelund::application {
enum class FeatureRuntimeStatus;
}

namespace voelund::desktop {

// U10 (user decision 2026-10-07): the red (error) or amber (blocked) marker of a feature in
// tree and timeline.
[[nodiscard]] QIcon featureStatusIcon(application::FeatureRuntimeStatus status);

// U10: what went wrong with the selected feature, above its properties, with the way out:
// edit or repair a failed sketch, or go to the failed feature that blocks this one.
class FeatureErrorCard final : public QFrame {
  public:
    struct Content final {
        bool blocked{false};
        QString message;
        QString cause;        // Blocked: name of the failed feature that blocks this one.
        bool canEdit{false};  // A failed sketch opens in the editor.
        bool canRepair{false}; // A failed sketch on a body face offers reference repair.
    };
    struct Handlers final {
        std::function<void()> openCause;
        std::function<void()> edit;
        std::function<void()> repair;
    };

    explicit FeatureErrorCard(QWidget* parent = nullptr);
    void setHandlers(Handlers handlers);
    void showError(const Content& content);
    void clear();

  private:
    QLabel* title_{nullptr};
    QLabel* message_{nullptr};
    QPushButton* cause_{nullptr};
    QPushButton* edit_{nullptr};
    QPushButton* repair_{nullptr};
    Handlers handlers_;
};

} // namespace voelund::desktop
