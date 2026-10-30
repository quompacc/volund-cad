// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "StepReadDialog.hpp"

#include <QEventLoop>
#include <QFileInfo>
#include <QMimeData>
#include <QObject>
#include <QProgressDialog>
#include <QTimer>
#include <QUrl>

#include <atomic>
#include <exception>
#include <thread>
#include <utility>

namespace voelund::desktop {

kernel::StepImport readStepFile(QWidget& parent, const std::filesystem::path& path) {
    std::atomic<int> permille{0};
    std::atomic<bool> stop{false};
    std::atomic<bool> finished{false};
    kernel::StepImport result;
    std::thread reader([&] {
        try {
            result = kernel::readStep(path, [&](double fraction) {
                permille = static_cast<int>(fraction * 1000);
                return !stop;
            });
        } catch (const std::exception& error) {
            result.error = kernel::StepImportError::unreadable;
            result.message = error.what();
        }
        finished = true;
    });
    QProgressDialog dialog(QObject::tr("STEP-Datei wird gelesen …"), QObject::tr("Abbrechen"), 0,
                           1000, &parent);
    dialog.setObjectName(QStringLiteral("stepReadProgress"));
    dialog.setWindowModality(Qt::WindowModal);
    dialog.setMinimumDuration(500);
    dialog.setAutoClose(false);
    dialog.setAutoReset(false);
    QObject::connect(&dialog, &QProgressDialog::canceled, &dialog, [&] {
        stop = true;
        dialog.setLabelText(QObject::tr("Wird abgebrochen …"));
    });
    QEventLoop loop;
    QTimer timer;
    QObject::connect(&timer, &QTimer::timeout, &loop, [&] {
        dialog.setValue(permille);
        if (finished)
            loop.quit();
    });
    timer.start(50);
    loop.exec();
    reader.join();
    return result;
}

QString droppedFile(const QMimeData& data) {
    for (const auto& url : data.urls()) {
        const QString file = url.toLocalFile();
        const QString suffix = QFileInfo(file).suffix().toLower();
        if (suffix == QStringLiteral("step") || suffix == QStringLiteral("stp") ||
            suffix == QStringLiteral("volund"))
            return file;
    }
    return {};
}

} // namespace voelund::desktop
