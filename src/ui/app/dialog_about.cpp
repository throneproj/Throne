#include "include/ui/app/dialog_about.h"

#include <QDesktopServices>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHash>
#include <QIcon>
#include <QLabel>
#include <QProcess>
#include <QPushButton>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include "NkrVersion.h"
#include "include/global/Configs.hpp"

DialogAbout::DialogAbout(QWidget *parent) : QDialog(parent) {
    setWindowTitle(tr("About Throne"));

    auto *icon = new QLabel(this);
    icon->setPixmap(QIcon(QStringLiteral(":/Throne/Throne.png")).pixmap(64));
    icon->setAlignment(Qt::AlignTop);

    auto *title = new QLabel(QStringLiteral("Throne"), this);
    auto titleFont = title->font();
    titleFont.setBold(true);
    if (titleFont.pointSizeF() > 0) {
        titleFont.setPointSizeF(titleFont.pointSizeF() * 1.5);
    } else {
        titleFont.setPixelSize(titleFont.pixelSize() * 3 / 2);
    }
    title->setFont(titleFont);

    const auto valueLabel = [this](const QString &text) {
        auto *label = new QLabel(text, this);
        label->setTextFormat(Qt::PlainText);
        label->setTextInteractionFlags(Qt::TextSelectableByMouse);
        return label;
    };
    auto *singBoxVersion = valueLabel(QStringLiteral("…"));
    auto *xrayVersion = valueLabel(QStringLiteral("…"));
    auto *goVersion = valueLabel(QStringLiteral("…"));

    auto *versions = new QFormLayout;
    versions->addRow(tr("Version:"), valueLabel(QStringLiteral(NKR_VERSION)));
    versions->addRow(QStringLiteral("sing-box:"), singBoxVersion);
    versions->addRow(QStringLiteral("Xray-core:"), xrayVersion);
    versions->addRow(QStringLiteral("Qt:"), valueLabel(QString::fromLatin1(qVersion())));
    versions->addRow(QStringLiteral("Go:"), goVersion);

    auto *description = new QLabel(tr("Open-source and cross platform Desktop Proxy management utility developed as a successor to Nekoray."), this);
    description->setWordWrap(true);
    description->setMinimumWidth(fontMetrics().averageCharWidth() * 50);

    auto *text = new QVBoxLayout;
    text->addWidget(title);
    text->addWidget(description);
    text->addSpacing(fontMetrics().height() / 2);
    text->addLayout(versions);

    auto *top = new QHBoxLayout;
    top->setSpacing(fontMetrics().height());
    top->addWidget(icon);
    top->addLayout(text, 1);

    auto *github = new QPushButton(QStringLiteral("GitHub"), this);
    github->setAutoDefault(false);
    connect(github, &QPushButton::clicked, this, [] {
        QDesktopServices::openUrl(QUrl(QStringLiteral("https://github.com/throneproj/Throne")));
    });
    auto *support = new QPushButton(tr("Support Throne"), this);
    support->setAutoDefault(false);
    connect(support, &QPushButton::clicked, this, [] {
        QDesktopServices::openUrl(QUrl(QStringLiteral("https://github.com/throneproj/Throne/blob/dev/.github/donate/Donate.md")));
    });

    auto *buttons = new QHBoxLayout;
    buttons->addStretch();
    buttons->addWidget(github);
    buttons->addWidget(support);
    buttons->addStretch();

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(top);
    layout->addSpacing(fontMetrics().height() / 2);
    layout->addLayout(buttons);
    layout->setSizeConstraint(QLayout::SetFixedSize);

    // A core predating --version still prints its sing-box and Xray lines before failing for want of a GUI socket.
    auto *core = new QProcess(this);
    core->setStandardInputFile(QProcess::nullDevice());
    const auto showCoreVersions = [=, this](const QByteArray &output) {
        QHash<QString, QString> found;
        for (const auto &line : QString::fromUtf8(output).split(QLatin1Char('\n'))) {
            const auto colon = line.indexOf(QLatin1String(": "));
            if (colon > 0) found.insert(line.left(colon), line.mid(colon + 2).trimmed());
        }
        const auto unknown = tr("Unknown");
        singBoxVersion->setText(found.value(QStringLiteral("sing-box"), unknown));
        xrayVersion->setText(found.value(QStringLiteral("Xray-core"), unknown));
        goVersion->setText(found.value(QStringLiteral("Go"), unknown));
    };
    connect(core, &QProcess::finished, this, [=] { showCoreVersions(core->readAllStandardOutput()); });
    connect(core, &QProcess::errorOccurred, this, [=](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) showCoreVersions({});
    });
    QTimer::singleShot(3000, core, [core] { core->kill(); });
    core->start(Configs::FindCoreRealPath(), {QStringLiteral("--version")});
}
