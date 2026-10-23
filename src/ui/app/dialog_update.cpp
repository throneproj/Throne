#include "include/ui/app/dialog_update.h"

#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QScreen>
#include <QTextBrowser>
#include <QVBoxLayout>

#include "NkrVersion.h"

DialogUpdate::DialogUpdate(const QString &version, bool preRelease, const QString &notes, const QUrl &releaseUrl,
                           bool canUpdate, const QString &notice, QWidget *parent) : QDialog(parent) {
    setWindowTitle(preRelease ? tr("Update (Pre-release)") : tr("Update"));

    auto *heading = new QLabel(tr("Throne %1 is available, you are using %2.").arg(version, QStringLiteral(NKR_VERSION)), this);
    heading->setTextFormat(Qt::PlainText);
    heading->setWordWrap(true);

    auto *notesView = new QTextBrowser(this);
    notesView->setOpenLinks(false);
    connect(notesView, &QTextBrowser::anchorClicked, this, [](const QUrl &url) {
        if (url.scheme() == QLatin1String("https") || url.scheme() == QLatin1String("http")) QDesktopServices::openUrl(url);
    });
    if (notes.isEmpty()) {
        notesView->setPlainText(tr("Release notes are unavailable."));
    } else {
        notesView->setMarkdown(notes);
    }

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(heading);
    layout->addWidget(notesView, 1);
    if (!notice.isEmpty()) {
        auto *noticeLabel = new QLabel(notice, this);
        noticeLabel->setTextFormat(Qt::PlainText);
        noticeLabel->setWordWrap(true);
        layout->addWidget(noticeLabel);
    }

    auto *buttons = new QDialogButtonBox(this);
    if (canUpdate) buttons->addButton(tr("Update", "button"), QDialogButtonBox::AcceptRole);
    auto *openInBrowser = buttons->addButton(tr("Open in browser"), QDialogButtonBox::ActionRole);
    buttons->addButton(tr("Close"), QDialogButtonBox::RejectRole);
    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
        emit updateRequested();
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(openInBrowser, &QPushButton::clicked, this, [this, releaseUrl] {
        QDesktopServices::openUrl(releaseUrl);
        accept();
    });
    layout->addWidget(buttons);

    QSize size(fontMetrics().averageCharWidth() * 80, fontMetrics().lineSpacing() * 28);
    if (const auto *screen = parent != nullptr ? parent->screen() : this->screen()) {
        size = size.boundedTo(screen->availableGeometry().size() * 0.8);
    }
    resize(size);
}
