#pragma once

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QFileDialog>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QThread>
#include <QTimer>
#include <QToolButton>

#include <functional>
#include <memory>
#include <stdexcept>
#include <string>

// U11: drives the window like a user: real mouse clicks on the buttons of rail, context bar,
// header, timeline and panel (found by the command ID of their action), the file menu, and
// answers to file dialogs and save prompts. No QAction::trigger() shortcuts.
namespace voelund::test {

inline void settleEvents() {
    for (int i = 0; i < 4; ++i)
        QApplication::processEvents();
}

// Window shortcuts (Escape, Enter of a tool panel) fire only in the active window, as when a
// user presses the key. Another window of the desktop may hold the activation; take it back.
inline void activate(QWidget& window) {
    for (int attempt = 0; attempt < 200 && QApplication::activeWindow() != &window; ++attempt) {
        window.raise();
        window.activateWindow();
        QThread::msleep(10);
        settleEvents();
    }
    if (QApplication::activeWindow() != &window)
        throw std::runtime_error("The window does not become active; keys cannot be tested");
}

// A left click at the widget's center, as the mouse would send it.
inline void press(QWidget& widget) {
    if (!widget.isVisible() || !widget.isEnabled()) {
        QString name = widget.objectName();
        if (auto* button = qobject_cast<QToolButton*>(&widget); button && button->defaultAction())
            name += button->defaultAction()->property("commandId").toString();
        throw std::runtime_error("Click on a hidden or disabled widget: " + name.toStdString() +
                                 (widget.isVisible() ? " (disabled)" : " (hidden)"));
    }
    const QPointF center = QRectF(widget.rect()).center();
    const QPointF global = widget.mapToGlobal(center);
    QMouseEvent down(QEvent::MouseButtonPress, center, global, Qt::LeftButton, Qt::LeftButton,
                     Qt::NoModifier);
    QMouseEvent up(QEvent::MouseButtonRelease, center, global, Qt::LeftButton, Qt::NoButton,
                   Qt::NoModifier);
    QApplication::sendEvent(&widget, &down);
    QApplication::sendEvent(&widget, &up);
    settleEvents();
}

// The visible tool button below `root` whose action carries `commandId`.
inline QToolButton& commandButton(QWidget& root, const char* commandId) {
    for (auto* button : root.findChildren<QToolButton*>())
        if (button->isVisible() && button->defaultAction() != nullptr &&
            button->defaultAction()->property("commandId").toString() ==
                QLatin1String(commandId))
            return *button;
    throw std::runtime_error(std::string("No visible button for ") + commandId);
}

// Opens `menuButton`'s popup and clicks the entry carrying `commandId` there.
inline void chooseFromMenu(QToolButton& menuButton, const char* commandId) {
    // The popup may appear later (Wayland), so the entry is looked for until it is there.
    bool chosen = false;
    int attempts = 0;
    auto attempt = std::make_shared<std::function<void()>>();
    *attempt = [&chosen, &attempts, attempt, id = QString::fromLatin1(commandId)] {
        auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
        if (menu == nullptr || !menu->isVisible()) {
            if (++attempts < 200)
                QTimer::singleShot(10, *attempt);
            return;
        }
        for (QAction* action : menu->actions())
            if (action->property("commandId").toString() == id) {
                const QPoint at = menu->actionGeometry(action).center();
                QMouseEvent down(QEvent::MouseButtonPress, at, menu->mapToGlobal(at),
                                 Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
                QMouseEvent up(QEvent::MouseButtonRelease, at, menu->mapToGlobal(at),
                               Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
                QApplication::sendEvent(menu, &down);
                QApplication::sendEvent(menu, &up);
                chosen = true;
                return;
            }
        menu->close();
    };
    QTimer::singleShot(0, *attempt);
    press(menuButton);
    for (int wait = 0; !chosen && wait < 300; ++wait) {
        QApplication::processEvents(QEventLoop::AllEvents, 10);
        if (QApplication::activePopupWidget() == nullptr && attempts >= 200)
            break;
    }
    settleEvents();
    if (!chosen)
        throw std::runtime_error(std::string("Menu entry missing: ") + commandId);
}

// Answers file dialogs with `file` (or cancels them when empty) and the save prompt with
// `answer`; error messages are acknowledged and counted.
struct DialogAnswers final {
    QString file;
    QMessageBox::StandardButton answer{QMessageBox::Cancel};
    int fileDialogs{0};
    int prompts{0};
    int errors{0};
    QTimer timer;
    DialogAnswers() {
        QObject::connect(&timer, &QTimer::timeout, [this] {
            auto* modal = QApplication::activeModalWidget();
            if (auto* dialog = qobject_cast<QFileDialog*>(modal)) {
                ++fileDialogs;
                if (file.isEmpty()) {
                    dialog->reject();
                    return;
                }
                dialog->selectFile(file);
                if (auto* edit = dialog->findChild<QLineEdit*>(QStringLiteral("fileNameEdit")))
                    edit->setText(file);
                QMetaObject::invokeMethod(dialog, "accept", Qt::DirectConnection);
            } else if (auto* message = qobject_cast<QMessageBox*>(modal)) {
                if (message->standardButtons().testFlag(QMessageBox::Discard)) {
                    ++prompts;
                    message->button(answer)->click();
                } else if (message->button(QMessageBox::Ok) != nullptr) {
                    ++errors;
                    message->button(QMessageBox::Ok)->click();
                }
            }
        });
        timer.start(30);
    }
};

} // namespace voelund::test
