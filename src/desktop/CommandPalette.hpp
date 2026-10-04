#pragma once

#include <QDialog>
#include <QString>

#include <optional>

class QLineEdit;
class QListWidget;

namespace voelund::desktop {

// U04: command search over every action of the window that carries a command ID (Ctrl+K,
// ADR 0012). It triggers the same QAction as the tool rail, the context bar and shortcuts.
class CommandPalette final : public QDialog {
  public:
    explicit CommandPalette(QWidget& window);

    void openSearch();
    // Runs a command by ID. Returns a diagnostic for an unknown or unavailable command.
    [[nodiscard]] static std::optional<QString> trigger(QWidget& window, const QString& id);

  protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

  private:
    void refill();
    void runCurrent();

    QWidget& window_;
    QLineEdit* query_{nullptr};
    QListWidget* results_{nullptr};
};

} // namespace voelund::desktop
