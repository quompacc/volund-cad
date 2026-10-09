#pragma once

#include <QApplication>
#include <QMessageBox>
#include <QPushButton>
#include <QString>
#include <QTimer>

#include <functional>
#include <memory>

namespace voelund::test {

// S13: answers the next "Änderungen an der Skizze verwerfen?" question with `button`
// ("Verwerfen" or "Weiter bearbeiten") once it is shown; `asked` records that it was.
inline void answerDiscardQuestion(const QString& button, bool* asked = nullptr) {
    auto attempt = std::make_shared<std::function<void()>>();
    *attempt = [button, asked, attempt] {
        auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        if (box == nullptr || box->objectName() != QStringLiteral("discardSketchQuestion")) {
            QTimer::singleShot(10, *attempt);
            return;
        }
        if (asked != nullptr)
            *asked = true;
        for (auto* candidate : box->buttons())
            if (candidate->text() == button) {
                candidate->click();
                return;
            }
    };
    QTimer::singleShot(0, *attempt);
}

} // namespace voelund::test
