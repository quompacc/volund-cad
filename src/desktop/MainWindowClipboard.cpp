#include "CommandFeedback.hpp"
#include "MainWindowState.hpp"
#include "MainWindowSupport.hpp"

#include <QAction>
#include <QClipboard>
#include <QDataStream>
#include <QGuiApplication>
#include <QIODevice>
#include <QMimeData>
#include <QStatusBar>

// N02 (user decision 2026-10-21): Strg+C copies the selected bodies, Strg+V pastes them as a
// fixed copy "Kopie N" (CadSession::pasteBodies). The solids travel through the system clipboard
// in Völund's own format, so a paste also works in another document or window.
namespace voelund::desktop {
namespace {

const QString bodiesFormat = QStringLiteral("application/x-voelund-bodies");
constexpr quint32 bodiesVersion = 1;

QByteArray encode(const std::vector<document::ImportedSolid>& solids) {
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream << bodiesVersion << static_cast<quint32>(solids.size());
    for (const auto& solid : solids)
        stream << QByteArray::fromStdString(solid.name)
               << QByteArray(solid.brep->data(), static_cast<qsizetype>(solid.brep->size()));
    return data;
}

// Throws std::invalid_argument for data that is not Völund bodies.
std::vector<document::ImportedSolid> decode(const QByteArray& data) {
    QDataStream stream(data);
    quint32 version = 0;
    quint32 count = 0;
    stream >> version >> count;
    if (stream.status() != QDataStream::Ok || version != bodiesVersion || count == 0 ||
        count > document::maximumImportedSolids)
        throw std::invalid_argument("The clipboard holds no Völund bodies");
    std::vector<document::ImportedSolid> solids;
    for (quint32 index = 0; index < count; ++index) {
        QByteArray name;
        QByteArray brep;
        stream >> name >> brep;
        if (stream.status() != QDataStream::Ok)
            throw std::invalid_argument("The clipboard's bodies are incomplete");
        solids.push_back(document::importedSolid(brep.toStdString(), name.toStdString()));
    }
    return solids;
}

} // namespace

void MainWindow::createClipboardCommands() {
    impl_->copyAction = new QAction(tr("Kopieren"), this);
    impl_->copyAction->setShortcut(QKeySequence::Copy);
    impl_->copyAction->setToolTip(tr("Gewählte Körper in die Zwischenablage kopieren (Strg+C)"));
    setCommandId(*impl_->copyAction, "edit.copy");
    addAction(impl_->copyAction);
    connect(impl_->copyAction, &QAction::triggered, this, [this] { copyBodies(); });
    impl_->pasteAction = new QAction(tr("Einfügen"), this);
    impl_->pasteAction->setShortcut(QKeySequence::Paste);
    impl_->pasteAction->setToolTip(tr("Kopierte Körper als feste Kopie einfügen (Strg+V)"));
    setCommandId(*impl_->pasteAction, "edit.paste");
    addAction(impl_->pasteAction);
    connect(impl_->pasteAction, &QAction::triggered, this, [this] { pasteBodies(); });
    connect(QGuiApplication::clipboard(), &QClipboard::dataChanged, this,
            [this] { syncClipboardCommands(); });
    syncClipboardCommands();
}

void MainWindow::syncClipboardCommands() {
    if (impl_->copyAction == nullptr)
        return;
    const ShellState shell = impl_->shell();
    const bool free = !shell.toolBlocksCommands();
    impl_->copyAction->setEnabled(free && !selectedBodies().empty());
    const QMimeData* clip = QGuiApplication::clipboard()->mimeData();
    impl_->pasteAction->setEnabled(free && clip != nullptr && clip->hasFormat(bodiesFormat));
}

void MainWindow::copyBodies() {
    const auto bodies = selectedBodies();
    if (bodies.empty())
        return;
    try {
        auto* clip = new QMimeData;
        clip->setData(bodiesFormat, encode(impl_->session.copyBodies(bodies)));
        clip->setText(tr("%n Völund-Körper", nullptr, static_cast<int>(bodies.size())));
        QGuiApplication::clipboard()->setMimeData(clip);
        statusBar()->showMessage(tr("%n Körper kopiert", nullptr, static_cast<int>(bodies.size())),
                                 3000);
    } catch (const std::exception& error) {
        statusBar()->showMessage(
            tr("Kopieren nicht möglich: %1").arg(QString::fromUtf8(error.what())));
    }
}

void MainWindow::pasteBodies() {
    const QMimeData* clip = QGuiApplication::clipboard()->mimeData();
    if (clip == nullptr || !clip->hasFormat(bodiesFormat))
        return;
    std::vector<document::ImportedSolid> solids;
    try {
        solids = decode(clip->data(bodiesFormat));
    } catch (const std::exception& error) {
        statusBar()->showMessage(
            tr("Einfügen nicht möglich: %1").arg(QString::fromUtf8(error.what())));
        return;
    }
    std::optional<document::FeatureId> pasted;
    if (!runCommand(
            [&] {
                auto result = impl_->session.pasteBodies(std::move(solids));
                pasted = result.feature();
                return result;
            },
            [this](const QString& message) { statusBar()->showMessage(message); }))
        return;
    refreshScene();
    if (pasted)
        selectBody(pasted->value());
    statusBar()->showMessage(tr("Als feste Kopie eingefügt"), 3000);
}

} // namespace voelund::desktop
