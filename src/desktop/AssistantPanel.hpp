#pragma once

#include <voelund/assistant/Assistant.hpp>

#include <QColor>
#include <QHash>
#include <QWidget>

#include <functional>

class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QToolButton;

namespace voelund::desktop {

// AI03b: the Claude panel at the right (user decision 2026-10-05). It shows the conversation
// with tool calls as compact lines, the selection that goes along as context, an input field,
// Send, Stop and New. It only presents; the window owns the backend and the document.
class AssistantPanel final : public QWidget {
  public:
    struct Handlers final {
        std::function<void(const QString& message, bool withSelection)> send;
        std::function<void()> stop;
        std::function<void()> reset;
    };

    explicit AssistantPanel(QWidget* parent = nullptr);
    void setHandlers(Handlers handlers) { handlers_ = std::move(handlers); }
    // Empty text: nothing selected.
    void setSelection(const QString& text);
    void setRunning(bool running);
    void present(const assistant::AssistantEvent& event);
    void addUserMessage(const QString& text);
    // Colors of tool lines, notes and errors follow the theme; existing lines are recolored.
    void setColors(const QColor& muted, const QColor& error);
    // Clears the conversation, e.g. after another document was opened.
    void clear(const QString& note = {});

  private:
    QListWidgetItem* addItem(const QString& text, const char* role);
    void submit();

    Handlers handlers_;
    QListWidget* history_{nullptr};
    QToolButton* selection_{nullptr};
    QLineEdit* input_{nullptr};
    QPushButton* send_{nullptr};
    QPushButton* stop_{nullptr};
    QPushButton* reset_{nullptr};
    QHash<QString, QListWidgetItem*> tools_; // Running tool calls by id.
    QColor muted_;
    QColor error_;
};

} // namespace voelund::desktop
