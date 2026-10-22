// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "AboutDialog.hpp"

#include "MainWindowSupport.hpp"
#include "RecoveryStore.hpp"

#include <voelund/BuildInfo.hpp>
#include <voelund/document/DocumentSerializer.hpp>

#include <QAction>
#include <QClipboard>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QSysInfo>
#include <QUrl>
#include <QVBoxLayout>
#include <Standard_Version.hxx>

namespace voelund::desktop {

QString diagnosticsText() {
    const auto line = [](const QString& label, const QString& value) {
        return label + QStringLiteral(": ") + value + QLatin1Char('\n');
    };
    QString text =
        QStringLiteral("Völund CAD ") + QString::fromLatin1(build::version) + QLatin1Char('\n');
    text += line(QObject::tr("Revision"), QString::fromLatin1(build::revision));
    text += line(QObject::tr("Lizenz"), QStringLiteral("GPL-3.0-or-later"));
    text += line(QObject::tr("Build"), QString::fromLatin1(build::buildType));
    text += line(QObject::tr("Dokumentformat"),
                 QObject::tr("Schema %1; liest ältere Schemata und VOLUND 1–16")
                     .arg(document::nativeDocumentSchema()));
    text += line(QStringLiteral("OpenCASCADE"), QStringLiteral(OCC_VERSION_COMPLETE));
    text += line(QStringLiteral("Qt"),
                 QObject::tr("%1 (gebaut mit %2)")
                     .arg(QString::fromLatin1(qVersion()), QStringLiteral(QT_VERSION_STR)));
    text += line(QObject::tr("Plattform"),
                 QStringLiteral("%1, %2 %3, %4")
                     .arg(QSysInfo::prettyProductName(), QSysInfo::kernelType(),
                          QSysInfo::kernelVersion(), QSysInfo::currentCpuArchitecture()));
    text += line(QObject::tr("Darstellung"), QGuiApplication::platformName());
    text += line(QObject::tr("Einstellungen"), QSettings().fileName());
    text += line(QObject::tr("Wiederherstellung"), RecoveryStore::defaultDirectory());
    return text;
}

namespace {

// LIZENZ (2026-10-22): the product licence, the missing warranty, and where the texts are: in
// the package next to bin/ (Windows and the Linux tarball alike).
QString licenceText() {
    const QDir root(QCoreApplication::applicationDirPath() + QStringLiteral("/.."));
    const auto link = [&root](const QString& path, const QString& label) {
        const QString file = root.absoluteFilePath(path);
        return QFileInfo::exists(file) ? QStringLiteral("<a href=\"%1\">%2</a>")
                                             .arg(QUrl::fromLocalFile(file).toString(), label)
                                       : label + QStringLiteral(" (") + path + QLatin1Char(')');
    };
    return QObject::tr("Lizenz: GNU General Public License, Version 3 oder später "
                       "(GPL-3.0-or-later).<br>Völund CAD wird ohne jede Gewährleistung "
                       "bereitgestellt.<br>%1 · %2")
        .arg(link(QStringLiteral("licenses/voelund/LICENSE"), QObject::tr("Lizenztext")),
             link(QStringLiteral("THIRD-PARTY-NOTICES.md"),
                  QObject::tr("Lizenzen der Komponenten")));
}

void showAbout(QWidget& parent) {
    QDialog dialog(&parent);
    dialog.setObjectName(QStringLiteral("aboutDialog"));
    dialog.setWindowTitle(QObject::tr("Über Völund"));
    auto* layout = new QVBoxLayout(&dialog);
    auto* heading =
        new QLabel(QStringLiteral("Völund CAD ") + QString::fromLatin1(build::version), &dialog);
    heading->setObjectName(QStringLiteral("aboutHeading"));
    layout->addWidget(heading);
    auto* licence = new QLabel(licenceText(), &dialog);
    licence->setObjectName(QStringLiteral("aboutLicence"));
    licence->setTextFormat(Qt::RichText);
    licence->setOpenExternalLinks(true);
    licence->setWordWrap(true);
    layout->addWidget(licence);
    auto* details = new QPlainTextEdit(diagnosticsText(), &dialog);
    details->setObjectName(QStringLiteral("aboutDiagnostics"));
    details->setReadOnly(true);
    details->setMinimumSize(520, 220);
    layout->addWidget(details);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    auto* copy = buttons->addButton(QObject::tr("Diagnose kopieren"), QDialogButtonBox::ActionRole);
    copy->setObjectName(QStringLiteral("aboutCopyDiagnostics"));
    QObject::connect(copy, &QPushButton::clicked, &dialog,
                     [details] { QGuiApplication::clipboard()->setText(details->toPlainText()); });
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    dialog.exec();
}

} // namespace

void addAboutAction(QMenu& menu, QWidget& parent) {
    menu.addSeparator();
    QAction* about = menu.addAction(QObject::tr("Über Völund…"));
    about->setObjectName(QStringLiteral("aboutAction"));
    setCommandId(*about, "app.about");
    QObject::connect(about, &QAction::triggered, &parent, [&parent] { showAbout(parent); });
}

} // namespace voelund::desktop
