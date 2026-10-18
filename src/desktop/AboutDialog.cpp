#include "AboutDialog.hpp"

#include "MainWindowSupport.hpp"
#include "RecoveryStore.hpp"

#include <voelund/BuildInfo.hpp>
#include <voelund/document/DocumentSerializer.hpp>

#include <QAction>
#include <QClipboard>
#include <QDialog>
#include <QDialogButtonBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QSysInfo>
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

void showAbout(QWidget& parent) {
    QDialog dialog(&parent);
    dialog.setObjectName(QStringLiteral("aboutDialog"));
    dialog.setWindowTitle(QObject::tr("Über Völund"));
    auto* layout = new QVBoxLayout(&dialog);
    auto* heading =
        new QLabel(QStringLiteral("Völund CAD ") + QString::fromLatin1(build::version), &dialog);
    heading->setObjectName(QStringLiteral("aboutHeading"));
    layout->addWidget(heading);
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
