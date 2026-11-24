#include "include/ui/setting/dialog_manage_routes.h"

#include <QClipboard>

#include "include/ui/utils/GuiUtils.hpp"

#include <QFile>
#include <QHostAddress>
#include <QMessageBox>
#include <QPointer>
#include <QShortcut>
#include <QTimer>
#include <QToolTip>
#include <QDialog>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QCheckBox>
#include <QScreen>
#include <QStyle>
#include <QGridLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QLabel>
#include <QRegularExpression>
#include <QDialogButtonBox>
#include <QApplication>
#include <QButtonGroup>
#include <QImage>
#include <QPainter>
#include <QStyledItemDelegate>

#include <algorithm>
#include <climits>
#include <cmath>

#include "include/configs/generate.h"
#include "include/configs/sub/warp.h"
#include "include/configs/sub/RouteUpdater.hpp"

#include "include/database/GroupsRepo.h"
#include "include/database/RoutesRepo.h"
#include "include/engine/services/ImportService.h"
#include "include/engine/services/RouteService.h"
#include "include/engine/services/SettingsService.h"
#include "include/ui/mainwindow.h"
#include "include/ui/setting/RawRouteItem.h"
#include "include/ui/setting/RouteDnsEditor.h"
#include "include/ui/setting/ThemeManager.hpp"
#include "include/ui/utils/MyPlainTextEdit.h"

namespace {
    // Mirrors the core's warp defaultAPIHosts.
    QStringList defaultWarpApiHosts() {
        return {QStringLiteral("api.cloudflareclient.com"), QStringLiteral("api.devices.cloudflare.com")};
    }

    constexpr int manageRoutesDefaultCacheCapacity = 65536;

    // The same trimming as the core's warp normalizeHosts.
    QString normalizeWarpApiHost(const QString &line) {
        static const QRegularExpression pathStart("[/?#]");
        QString host = line.trimmed().toLower();
        if (host.startsWith("https://")) host.remove(0, 8);
        else if (host.startsWith("http://")) host.remove(0, 7);
        if (const auto end = host.indexOf(pathStart); end >= 0) host.truncate(end);
        return host;
    }

    QStringList normalizeWarpApiHosts(const QString &text) {
        QStringList hosts;
        for (const auto &line : text.split('\n')) {
            const auto host = normalizeWarpApiHost(line);
            if (!host.isEmpty() && !hosts.contains(host)) hosts << host;
        }
        return hosts;
    }

    // Names and notes from shared or fetched profiles must not render as rich text.
    void manageRoutesPlainBox(QWidget *parent, QMessageBox::Icon icon, const QString &title, const QString &text, const QString &details = {}) {
        QMessageBox box(icon, title, text, QMessageBox::Ok, parent);
        box.setTextFormat(Qt::PlainText);
        if (!details.isEmpty()) box.setDetailedText(details);
        box.exec();
    }

    // Shown before import: those servers would see every query made while the profile is in use.
    QString manageRoutesDnsNote(const Configs::RouteDns &dns) {
        return DialogManageRoutes::tr("It uses its own DNS servers: remote %1, direct %2.").arg(dns.remote_dns, dns.direct_dns);
    }

    // ProfilesRepo::AddProfile files them under the group the main window shows.
    QString manageRoutesEndpointsNote(int count) {
        const auto group = Configs::dataManager->groupsRepo->CurrentGroup();
        return DialogManageRoutes::tr("Its endpoints will be added as %n new profile(s) to the group \"%1\".", nullptr, count)
            .arg(group != nullptr ? group->name : QString());
    }

    bool manageRoutesEndpointTargetAccepted(const QString &title) {
        const auto refusal = Engine::ImportService::CheckTarget(Configs::dataManager->settingsRepo->current_group);
        if (refusal) MessageBoxWarning(title, ImportTargetRefusalText(*refusal));
        return !refusal;
    }

    // FromShareInput adds the endpoint profiles through ProfilesRepo itself, so no ProfilesChanged refreshes the main window's list.
    void manageRoutesRefreshProxyList() {
        if (auto *window = GetMainWindow()) window->refresh_proxy_list({}, true);
    }

    // dns-local resolves the other servers' names, so it cannot depend on one itself.
    QString manageRoutesLocalDnsProblem(const QString &text) {
        if (text.isEmpty()) return {};
        Configs::DnsAddress parsed;
        QString why;
        if (!Configs::ParseDnsAddress(text, &parsed, &why)) return why;
        if (parsed.type == "local" || parsed.type == "dhcp" || Validation::Ip(parsed.server).isEmpty()) return {};
        return DialogManageRoutes::tr("Use an IP address, for example 223.5.5.5 or tls://1.1.1.1: this server resolves the names of the others.");
    }

    QString manageRoutesReservedProblem(const QString &text) {
        const auto items = Validation::Items(text, Validation::Split::Comma);
        if (items.isEmpty()) return {};
        const bool bytes = items.size() == 3 && std::all_of(items.begin(), items.end(), [](const QString &item) {
            return Validation::Int(item, 0, 255).isEmpty();
        });
        return bytes ? QString() : DialogManageRoutes::tr("Use three numbers from 0 to 255, for example 12,34,56.");
    }

    QString manageRoutesWarpMissing(const QString &text) {
        return text.isEmpty() ? DialogManageRoutes::tr("Required while WARP is on: click Generate WARP config, or fill it in.") : QString();
    }

    // The fields the build refuses to start without while WARP is on; it ignores them all while WARP is off.
    Validation::Rule manageRoutesWarpField(Validation::Rule rule) {
        return [rule = std::move(rule)](const QString &text) {
            if (const auto missing = manageRoutesWarpMissing(text); !missing.isEmpty()) return missing;
            return rule ? rule(text) : QString();
        };
    }

    constexpr int warpFragmentHandshake = 1;
    constexpr int warpFragmentRecords = 2;

    // The core takes interface addresses as prefixes only.
    QStringList manageRoutesWithPrefix(QStringList addresses) {
        for (auto &address : addresses) {
            // Unchecked while WARP is off, so only a real address gets a prefix.
            if (address.contains('/') || !Validation::Ip(address).isEmpty()) continue;
            address += Validation::Ip(address, Validation::Family::V4).isEmpty() ? "/32" : "/128";
        }
        return addresses;
    }

    // ParsePredefinedDNS's grammar: it hands the core the address as QHostAddress normalises it, zone dropped.
    QString manageRoutesPredefinedError(const QString &rawLine) {
        QString line = rawLine;
        if (const auto hash = line.indexOf('#'); hash != -1) line = line.left(hash);
        const auto fields = line.simplified().split(' ', Qt::SkipEmptyParts);
        if (fields.isEmpty()) return {};
        if (QHostAddress address; !address.setAddress(fields[0])) {
            const auto problem = Validation::Ip(fields[0]);
            return problem.isEmpty() ? DialogManageRoutes::tr("\"%1\" is not an IP address.").arg(fields[0]) : problem;
        }
        if (fields.size() < 2) return DialogManageRoutes::tr("Add at least one host name after the address, for example 10.0.0.5 nas.lan.");
        for (qsizetype i = 1; i < fields.size(); i++) {
            auto domain = fields[i];
            while (domain.endsWith('.')) domain.chop(1);
            if (domain.isEmpty()) return DialogManageRoutes::tr("\"%1\" is not a host name.").arg(fields[i]);
        }
        return {};
    }

    QString manageRoutesPredefinedWarning(const QString &text) {
        const QStringList lines = text.split('\n');
        for (qsizetype n = 0; n < lines.size(); n++) {
            if (!manageRoutesPredefinedError(lines[n]).isEmpty()) continue;
            QString line = lines[n];
            if (const auto hash = line.indexOf('#'); hash != -1) line = line.left(hash);
            const auto fields = line.simplified().split(' ', Qt::SkipEmptyParts);
            for (qsizetype i = 1; i < fields.size(); i++) {
                auto domain = fields[i];
                while (domain.endsWith('.')) domain.chop(1);
                if (const auto problem = Validation::Host(domain); !problem.isEmpty()) {
                    return DialogManageRoutes::tr("Line %1: \"%2\" never matches: %3").arg(QString::number(n + 1), domain, problem);
                }
            }
        }
        return {};
    }

    QColor manageRoutesMix(const QColor &top, const QColor &bottom, const float amount) {
        return QColor::fromRgbF(top.redF() * amount + bottom.redF() * (1 - amount), top.greenF() * amount + bottom.greenF() * (1 - amount),
                                top.blueF() * amount + bottom.blueF() * (1 - amount));
    }

    // What the mark sits on, as the style paints the row over the list's base: a selected row may be filled with the accent itself.
    QColor manageRoutesBackdrop(const QStyleOptionViewItem &item, const QPoint &at, const QStyle *style, const QWidget *widget) {
        QStyleOptionViewItem row(item);
        row.state &= ~QStyle::State_HasFocus;
        row.styleObject = nullptr;
        QImage sample(1, 1, QImage::Format_ARGB32_Premultiplied);
        sample.fill(item.palette.color(QPalette::Base));
        QPainter painter(&sample);
        painter.translate(-at.x(), -at.y());
        style->drawControl(QStyle::CE_ItemViewItem, &row, &painter, widget);
        painter.end();
        return sample.pixelColor(0, 0);
    }

    // Only one profile is in use, so its mark is a radio: a solid accent disc, or a muted ring.
    // Drawn where the style puts the check box, which keeps the stock hit area, Space toggling and the text's place.
    class ManageRoutesRadioDelegate : public QStyledItemDelegate {
    public:
        using QStyledItemDelegate::QStyledItemDelegate;

        void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
            QStyleOptionViewItem item(option);
            initStyleOption(&item, index);
            if (!(item.features & QStyleOptionViewItem::HasCheckIndicator) || (item.features & QStyleOptionViewItem::HasDecoration)) {
                QStyledItemDelegate::paint(painter, option, index);
                return;
            }
            const QWidget *widget = item.widget;
            QStyle *style = widget != nullptr ? widget->style() : QApplication::style();
            const QSize indicator(style->pixelMetric(QStyle::PM_IndicatorWidth, &item, widget), style->pixelMetric(QStyle::PM_IndicatorHeight, &item, widget));
            const int side = qMin(indicator.width(), indicator.height());
            // Centred on the check box's own rect: that is where a click toggles.
            const QRect box = QStyle::alignedRect(item.direction, Qt::AlignCenter, QSize(side, side),
                                                  style->subElementRect(QStyle::SE_ItemViewItemCheckIndicator, &item, widget));
            const bool checked = item.checkState == Qt::Checked;
            item.features &= ~QStyleOptionViewItem::HasCheckIndicator;
            item.features |= QStyleOptionViewItem::HasDecoration;
            item.decorationSize = indicator;
            item.icon = QIcon();
            style->drawControl(QStyle::CE_ItemViewItem, &item, painter, widget);

            const QColor backdrop = manageRoutesBackdrop(item, box.center(), style, widget);
            const auto &tokens = themeManager()->tokens;
            const QColor surface = tokens.surface.isValid() ? tokens.surface : item.palette.color(QPalette::Base);
            const QColor onSurface = tokens.onSurface.isValid() ? tokens.onSurface : item.palette.color(QPalette::Text);
            const QColor accent = tokens.accent.isValid() ? tokens.accent : item.palette.color(QPalette::Highlight);

            // Edges and stroke on whole device pixels keep the circle crisp at fractional scales.
            const qreal ratio = painter->device() != nullptr ? painter->device()->devicePixelRatioF() : 1.0;
            const auto snap = [ratio](const qreal at) { return std::round(at * ratio) / ratio; };
            const QRectF circle(QPointF(snap(box.left()), snap(box.top())), QPointF(snap(box.left() + side), snap(box.top() + side)));
            painter->save();
            painter->setRenderHint(QPainter::Antialiasing);
            if (!(item.state & QStyle::State_Enabled)) painter->setOpacity(0.4);
            if (checked) {
                painter->setPen(Qt::NoPen);
                painter->setBrush(ContrastSeparated(accent, backdrop, 3.0));
                painter->drawEllipse(circle);
            } else {
                const qreal stroke = qMax(1.0, std::round(1.2 * ratio)) / ratio;
                painter->setPen(QPen(ContrastSeparated(manageRoutesMix(onSurface, surface, 0.55f), backdrop, 3.0), stroke));
                painter->setBrush(Qt::NoBrush);
                painter->drawEllipse(circle.adjusted(stroke / 2, stroke / 2, -stroke / 2, -stroke / 2));
            }
            painter->restore();
        }
    };
}

void DialogManageRoutes::reloadProfileItems() {
    if (chainList.empty()) {
        MessageBoxWarning(tr("Invalid state"), tr("The list of routing profiles is empty, this should be an unreachable state, crashes may occur now"));
        return;
    }

    if (!chainList.contains(currentRoute)) currentRoute = chainList[0];
    // Or itemChanged would see the half-built list.
    const QSignalBlocker blocker(ui->route_profiles);
    ui->route_profiles->clear();
    for (const auto &item: chainList) {
        QListWidgetItem *row = nullptr;
        if (const auto owner = Configs::dataManager->groupsRepo->GetGroup(item->source_gid); owner != nullptr && owner->import_sub_route) {
            row = new QListWidgetItem(tr("%1 (updated by subscription \"%2\")").arg(item->name, owner->name), ui->route_profiles);
            row->setToolTip(tr("Each update of \"%1\" overwrites this profile's rules.").arg(owner->name));
        } else {
            row = new QListWidgetItem(item->name, ui->route_profiles);
        }
        row->setFlags(row->flags() | Qt::ItemIsUserCheckable);
        row->setCheckState(item == currentRoute ? Qt::Checked : Qt::Unchecked);
    }
}

// The checked profile is the one in use, so checking another moves the mark and unchecking it puts it back.
void DialogManageRoutes::syncRouteChecks(QListWidgetItem *changed) {
    const int row = ui->route_profiles->row(changed);
    if (row < 0 || row >= chainList.size()) return;
    if (changed->checkState() == Qt::Checked) currentRoute = chainList[row];
    const QSignalBlocker blocker(ui->route_profiles);
    for (int i = 0; i < ui->route_profiles->count() && i < chainList.size(); ++i) {
        ui->route_profiles->item(i)->setCheckState(chainList[i] == currentRoute ? Qt::Checked : Qt::Unchecked);
    }
}

void DialogManageRoutes::show_predefined_dns_editor() {
    auto w = new QDialog(this);
    w->setWindowTitle(tr("Predefined Answers"));
    w->setWindowModality(Qt::ApplicationModal);

    auto layout = new QGridLayout(w);

    auto enable = new QCheckBox(tr("Enable predefined answers"), w);
    enable->setChecked(predefined_dns_enabled);
    layout->addWidget(enable, 0, 0);

    auto tEdit = new QPlainTextEdit(w);
    tEdit->setPlaceholderText(tr("One entry per line, hosts-file syntax:\n\n"
                                 "127.0.0.1 localhost\n"
                                 "10.0.0.5 nas.lan files.lan\n"
                                 "::1 localhost6\n\n"
                                 "A domain listed with only one address family is answered "
                                 "NXDOMAIN for the other, so the override cannot be bypassed."));
    tEdit->setPlainText(predefined_dns_text);
    tEdit->setEnabled(predefined_dns_enabled);
    layout->addWidget(tEdit, 1, 0);

    connect(enable, &QCheckBox::stateChanged, tEdit, [=](int state) {
        tEdit->setEnabled(state != Qt::Unchecked);
    });

    auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, w);
    layout->addWidget(buttons, 2, 0);

    auto popupForm = new Validation::Form(w);
    popupForm->attachStatus(buttons);
    popupForm->bindLines(tEdit, manageRoutesPredefinedError).label(w->windowTitle());
    popupForm->bind(tEdit, manageRoutesPredefinedWarning).label(w->windowTitle()).warning().raw();
    popupForm->markAll();

    connect(buttons, &QDialogButtonBox::accepted, w, [=, this] {
        if (!popupForm->validateAll()) return;
        predefined_dns_enabled = enable->isChecked();
        predefined_dns_text = tEdit->toPlainText();
        w->accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, w, &QDialog::reject);

    w->resize(w->sizeHint().expandedTo(QSize(520, 360)).boundedTo(screen()->availableGeometry().size()));
    w->exec();
    w->deleteLater();
}

void DialogManageRoutes::show_dns_advanced_editor() {
    auto w = new QDialog(this);
    w->setWindowTitle(tr("Advanced DNS Settings"));
    w->setWindowModality(Qt::ApplicationModal);

    auto layout = new QFormLayout(w);

    auto cacheCap = new QLineEdit(Int2String(dns_advanced.cache_capacity), w);
    cacheCap->setValidator(QRegExpValidator_Number);
    cacheCap->setPlaceholderText(tr("%1 (default)").arg(manageRoutesDefaultCacheCapacity));
    layout->addRow(tr("Cache Capacity"), cacheCap);

    auto queryTimeout = new QLineEdit(dns_advanced.query_timeout, w);
    queryTimeout->setPlaceholderText(tr("10s (default)"));
    layout->addRow(tr("Query Timeout"), queryTimeout);

    auto optimistic = new QCheckBox(tr("Optimistic cache"), w);
    optimistic->setToolTip(tr("Serve expired answers while refreshing them in the background."));
    optimistic->setChecked(dns_advanced.optimistic);
    layout->addRow(optimistic);

    auto optimisticTimeout = new QLineEdit(dns_advanced.optimistic_timeout, w);
    optimisticTimeout->setPlaceholderText(tr("3d (default)"));
    layout->addRow(tr("Optimistic Timeout"), optimisticTimeout);

    auto disableCache = new QCheckBox(tr("Disable cache"), w);
    disableCache->setChecked(dns_advanced.disable_cache);
    layout->addRow(disableCache);

    auto disableExpire = new QCheckBox(tr("Disable expire"), w);
    disableExpire->setChecked(dns_advanced.disable_expire);
    layout->addRow(disableExpire);

    auto persistCache = new QCheckBox(tr("Save cache to file"), w);
    persistCache->setToolTip(tr("Keep cached DNS answers and FakeIP mappings across restarts."));
    persistCache->setChecked(dns_advanced.persist_cache);
    layout->addRow(persistCache);

    auto reverseMapping = new QCheckBox(tr("Reverse mapping"), w);
    reverseMapping->setChecked(dns_advanced.reverse_mapping);
    layout->addRow(reverseMapping);

    // The core rejects optimistic alongside either cache switch, so the UI keeps them exclusive.
    auto syncConflicts = [=] {
        const bool cacheOff = disableCache->isChecked() || disableExpire->isChecked();
        if (cacheOff) optimistic->setChecked(false);
        optimistic->setEnabled(!cacheOff);
        optimisticTimeout->setEnabled(optimistic->isChecked());
        disableCache->setEnabled(!optimistic->isChecked());
        disableExpire->setEnabled(!optimistic->isChecked());
        // Disable Cache short-circuits the store path, so the file would never be written.
        persistCache->setEnabled(!disableCache->isChecked());
    };
    connect(optimistic, &QCheckBox::toggled, w, syncConflicts);
    connect(disableCache, &QCheckBox::toggled, w, syncConflicts);
    connect(disableExpire, &QCheckBox::toggled, w, syncConflicts);
    syncConflicts();

    auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, w);
    layout->addRow(buttons);

    // Empty and 0 mean the default.
    auto popupForm = new Validation::Form(w);
    popupForm->attachStatus(buttons);
    popupForm->bind(cacheCap, Validation::Rules::Int(0, INT_MAX));
    popupForm->bind(cacheCap, [](const QString &text) {
        const auto value = Validation::ToInt(text);
        return value && *value >= 1 && *value < 1024 ? tr("The core raises values below 1024 to 1024.") : QString();
    }).warning();
    popupForm->bind(queryTimeout, Validation::Rules::Duration());
    popupForm->bind(optimisticTimeout, Validation::Rules::Duration());
    popupForm->markAll();

    connect(buttons, &QDialogButtonBox::accepted, w, [=, this] {
        if (!popupForm->validateAll()) return;
        const auto capacity = cacheCap->text().trimmed();
        dns_advanced.cache_capacity = capacity.toInt() == 0 ? manageRoutesDefaultCacheCapacity : capacity.toInt();
        dns_advanced.query_timeout = queryTimeout->text().trimmed();
        dns_advanced.optimistic = optimistic->isChecked();
        dns_advanced.optimistic_timeout = optimisticTimeout->text().trimmed();
        dns_advanced.disable_cache = disableCache->isChecked();
        dns_advanced.disable_expire = disableExpire->isChecked();
        dns_advanced.persist_cache = persistCache->isChecked();
        dns_advanced.reverse_mapping = reverseMapping->isChecked();
        w->accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, w, &QDialog::reject);

    w->resize(w->sizeHint().boundedTo(screen()->availableGeometry().size()));
    w->exec();
    w->deleteLater();
}

void DialogManageRoutes::show_dns_object_editor() {
    auto w = new QDialog(this);
    w->setWindowTitle(tr("DNS Object"));
    w->setWindowModality(Qt::ApplicationModal);

    auto layout = new QGridLayout(w);

    auto tEdit = new QPlainTextEdit(w);
    tEdit->setPlaceholderText(DecodeB64IfValid("ewogICJzZXJ2ZXJzIjogW10sCiAgInJ1bGVzIjogW10sCiAgImZpbmFsIjogIiIsCiAgInN0cmF0ZWd5IjogIiIsCiAgImRpc2FibGVfY2FjaGUiOiBmYWxzZSwKICAiZGlzYWJsZV9leHBpcmUiOiBmYWxzZSwKICAiaW5kZXBlbmRlbnRfY2FjaGUiOiBmYWxzZSwKICAicmV2ZXJzZV9tYXBwaW5nIjogZmFsc2UsCiAgImZha2VpcCI6IHt9Cn0="));
    tEdit->setPlainText(dns_object_text);
    layout->addWidget(tEdit, 0, 0, 1, 2);

    auto format = new QPushButton(tr("Format"), w);
    layout->addWidget(format, 1, 0);
    connect(format, &QPushButton::clicked, w, [=] {
        auto obj = QString2QJsonObject(tEdit->toPlainText());
        if (obj.isEmpty()) {
            MessageBoxWarning(tr("Invalid input"), tr("The DNS object is not a valid JSON object"));
            return;
        }
        tEdit->setPlainText(QJsonObject2QString(obj, false));
    });

    auto document = new QPushButton(tr("Document"), w);
    layout->addWidget(document, 1, 1);
    connect(document, &QPushButton::clicked, w, [=] {
        MessageBoxInfo("DNS", "https://sing-box.sagernet.org/configuration/dns/");
    });

    auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, w);
    layout->addWidget(buttons, 2, 0, 1, 2);

    auto popupForm = new Validation::Form(w);
    popupForm->attachStatus(buttons);
    popupForm->bind(tEdit, Validation::Rules::JsonObject()).label(w->windowTitle()).raw();
    popupForm->markAll();

    connect(buttons, &QDialogButtonBox::accepted, w, [=, this] {
        if (!popupForm->validateAll()) return;
        dns_object_text = tEdit->toPlainText();
        w->accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, w, &QDialog::reject);

    w->resize(w->sizeHint().expandedTo(QSize(560, 420)).boundedTo(screen()->availableGeometry().size()));
    w->exec();
    w->deleteLater();
    form->revalidate(ui->use_dns_object);
}

void DialogManageRoutes::show_warp_advanced_editor() {
    auto w = new QDialog(this);
    w->setWindowTitle(tr("Advanced WARP Settings"));
    w->setWindowModality(Qt::ApplicationModal);

    auto layout = new QVBoxLayout(w);
    const auto addRow = [](QGridLayout *grid, int row, const QString &text, QWidget *field) {
        auto label = new QLabel(text, grid->parentWidget());
        label->setBuddy(field);
        grid->addWidget(label, row, 0);
        grid->addWidget(field, row, 1);
        return label;
    };
    const auto addHint = [](QGridLayout *grid, int row, const QString &text) {
        auto hint = new QLabel(text, grid->parentWidget());
        hint->setWordWrap(true);
        hint->setProperty("colorRole", QStringLiteral("muted"));
        grid->addWidget(hint, row, 1);
    };

    const bool masque = ui->warp_mode->currentIndex() == 1;
    const WarpAdvancedDraft opened = warp_advanced;

    auto wireguardBox = new QGroupBox(QStringLiteral("WireGuard"), w);
    auto wireguardGrid = new QGridLayout(wireguardBox);
    auto privateKey = new QLineEdit(opened.private_key, wireguardBox);
    addRow(wireguardGrid, 0, tr("Private Key"), privateKey);
    auto publicKey = new QLineEdit(opened.public_key, wireguardBox);
    addRow(wireguardGrid, 1, tr("Public Key"), publicKey);
    auto reserved = new QLineEdit(opened.reserved, wireguardBox);
    addRow(wireguardGrid, 2, tr("Reserved"), reserved);
    wireguardBox->setHidden(masque);
    layout->addWidget(wireguardBox);

    auto masqueBox = new QGroupBox(QStringLiteral("MASQUE"), w);
    auto masqueGrid = new QGridLayout(masqueBox);
    auto masquePrivateKey = new QLineEdit(opened.masque_private_key, masqueBox);
    addRow(masqueGrid, 0, tr("Private Key"), masquePrivateKey);
    auto peerPublicKey = new QLineEdit(opened.masque_peer_public_key, masqueBox);
    addRow(masqueGrid, 1, tr("Peer Public Key"), peerPublicKey)
        ->setToolTip(tr("Base64 DER of the server's public key. When set, it pins the server certificate."));
    auto sni = new QLineEdit(opened.masque_sni, masqueBox);
    addRow(masqueGrid, 2, QStringLiteral("SNI"), sni);
    auto httpMode = new QComboBox(masqueBox);
    httpMode->addItems({tr("HTTP/3 (fallback to HTTP/2)"), tr("HTTP/3 only"), QStringLiteral("HTTP/2")});
    httpMode->setCurrentIndex(opened.masque_http_mode);
    httpMode->setToolTip(tr("HTTP/3 runs over UDP; HTTP/2 runs over TLS on TCP and works where UDP is blocked."));
    addRow(masqueGrid, 3, tr("HTTP Version"), httpMode)->setToolTip(httpMode->toolTip());
    masqueBox->setHidden(!masque);
    layout->addWidget(masqueBox);

    const auto *settings = Configs::dataManager->settingsRepo.get();
    auto registration = new QGroupBox(tr("Registration"), w);
    auto registrationGrid = new QGridLayout(registration);
    auto hostsEdit = new MyPlainTextEdit(registration);
    hostsEdit->setVisibleLines(4);
    hostsEdit->setPlainText((settings->warp_api_hosts.isEmpty() ? defaultWarpApiHosts() : settings->warp_api_hosts).join('\n'));
    auto hostsLabel = new QLabel(tr("Registration Domains"), registration);
    hostsLabel->setBuddy(hostsEdit);
    registrationGrid->addWidget(hostsLabel, 0, 0, Qt::AlignTop);
    registrationGrid->addWidget(hostsEdit, 0, 1);
    addHint(registrationGrid, 1, tr("One domain per line. They are tried in order and the first one that accepts the registration is used. "
                                    "Leave empty to use %1.").arg(defaultWarpApiHosts().join(", ")));
    auto fragment = new QComboBox(registration);
    fragment->addItem(tr("Off"), 0);
    fragment->addItem(tr("Handshake"), warpFragmentHandshake);
    fragment->addItem(tr("Records"), warpFragmentRecords);
    fragment->addItem(tr("Handshake + Records"), warpFragmentHandshake | warpFragmentRecords);
    fragment->setCurrentIndex(fragment->findData((settings->warp_api_tls_fragment ? warpFragmentHandshake : 0)
                                                 | (settings->warp_api_tls_record_fragment ? warpFragmentRecords : 0)));
    addRow(registrationGrid, 2, tr("Fragment TLS"), fragment);
    auto fallbackDelay = new QLineEdit(settings->warp_api_tls_fragment_fallback_delay, registration);
    fallbackDelay->setPlaceholderText(QStringLiteral("500ms"));
    auto fallbackDelayLabel = addRow(registrationGrid, 3, tr("Fallback delay"), fallbackDelay);
    addHint(registrationGrid, 4, tr("Applies only when registration connects directly, not through a running profile."));
    const auto syncFallbackDelay = [=] {
        const bool handshake = (fragment->currentData().toInt() & warpFragmentHandshake) != 0;
        fallbackDelayLabel->setEnabled(handshake);
        fallbackDelay->setEnabled(handshake);
    };
    connect(fragment, &QComboBox::currentIndexChanged, w, syncFallbackDelay);
    syncFallbackDelay();
    layout->addWidget(registration);

    auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, w);
    layout->addWidget(buttons);
    ADD_ASTERISK(w)

    const auto warpOn = [this] { return ui->exit_warp->isChecked(); };
    const auto warpKey = Validation::Rules::Base64Key(32, false, true, Validation::Empty::Allowed);
    auto popupForm = new Validation::Form(w);
    popupForm->attachStatus(buttons);
    // Missing values only warn here, so registration can be set up before the first generate; accept() refuses them.
    for (auto *edit : {privateKey, publicKey, masquePrivateKey, peerPublicKey})
        popupForm->bind(edit, manageRoutesWarpMissing).when(warpOn).warning();
    popupForm->bind(privateKey, warpKey).when(warpOn);
    popupForm->bind(publicKey, warpKey).when(warpOn);
    popupForm->bind(reserved, manageRoutesReservedProblem).when(warpOn);
    popupForm->bindLines(hostsEdit, [](const QString &line) {
        const auto host = normalizeWarpApiHost(line);
        if (host.isEmpty()) return QString();
        // The core requests "https://" + host, where a bare IPv6 address reads as host:port.
        if (host.count(':') > 1 && !host.startsWith('['))
            return tr("Put an IPv6 address in brackets, for example [2606:4700::1].");
        return Validation::HostPort(host, false);
    });
    popupForm->bind(fallbackDelay, Validation::Rules::Duration());
    popupForm->markAll();

    connect(buttons, &QDialogButtonBox::accepted, w, [=, this] {
        if (!popupForm->validateAll()) return;
        auto hosts = normalizeWarpApiHosts(hostsEdit->toPlainText());
        // Saving the defaults verbatim would freeze them and miss later changes to the built-in list.
        if (hosts == defaultWarpApiHosts()) hosts.clear();
        const int split = fragment->currentData().toInt();
        const bool tlsFragment = (split & warpFragmentHandshake) != 0;
        const bool recordFragment = (split & warpFragmentRecords) != 0;
        const auto delay = fallbackDelay->text().trimmed();
        // Generate WARP config reads these from the settings, so they are saved now, not with the dialog.
        if (hosts != settings->warp_api_hosts || tlsFragment != settings->warp_api_tls_fragment
            || recordFragment != settings->warp_api_tls_record_fragment || delay != settings->warp_api_tls_fragment_fallback_delay) {
            const auto error = Engine::SettingsService::Update([&](Configs::SettingsValues &values) {
                values.warp_api_hosts = hosts;
                values.warp_api_tls_fragment = tlsFragment;
                values.warp_api_tls_record_fragment = recordFragment;
                values.warp_api_tls_fragment_fallback_delay = delay;
            }, {.interactive = true});
            if (error) {
                MessageBoxWarning(w->windowTitle(), SettingsApplyErrorText(*error));
                return;
            }
        }
        // Edited fields only: a Generate WARP config that finished while this was open keeps the rest.
        const auto keep = [](QString &value, const QString &before, const QString &now) {
            if (now != before) value = now;
        };
        keep(warp_advanced.private_key, opened.private_key, privateKey->text());
        keep(warp_advanced.public_key, opened.public_key, publicKey->text());
        keep(warp_advanced.reserved, opened.reserved, reserved->text());
        keep(warp_advanced.masque_private_key, opened.masque_private_key, masquePrivateKey->text());
        keep(warp_advanced.masque_peer_public_key, opened.masque_peer_public_key, peerPublicKey->text());
        keep(warp_advanced.masque_sni, opened.masque_sni, sni->text());
        if (httpMode->currentIndex() != opened.masque_http_mode) warp_advanced.masque_http_mode = httpMode->currentIndex();
        w->accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, w, &QDialog::reject);

    w->resize(w->sizeHint().expandedTo(QSize(560, 0)).boundedTo(screen()->availableGeometry().size()));
    w->exec();
    w->deleteLater();
    form->revalidate(ui->warp_advanced_btn);
}

DialogManageRoutes::DialogManageRoutes(QWidget *parent) : QDialog(parent), ui(new Ui::DialogManageRoutes) {
    snapshot_ = *Configs::dataManager->settingsRepo;
    ui->setupUi(this);
    form = new Validation::Form(this);
    form->attachStatus(ui->buttonBox);
    auto profiles =Configs::dataManager->routesRepo->GetAllRouteProfiles();
    for (const auto &item: profiles) {
        chainList << item;
    }
    if (chainList.empty()) {
        auto defaultChain = Configs::RouteProfile::GetDefaultChain();
        if (const auto error = Engine::RouteService::Add(defaultChain)) MW_show_log(error->message);
        chainList.append(defaultChain);
    }
    currentRoute = Configs::dataManager->routesRepo->GetRouteProfile(snapshot_.current_route_id);
    if (currentRoute == nullptr) currentRoute = chainList[0];

    // Both strategy pickers must stay on one shared item order.
    ui->default_domain_strategy->addItems(Configs::DomainStrategy::DomainStrategy);
    ui->domainStrategyCombo->addItems(Configs::DomainStrategy::DomainStrategy);
    ui->local_override->setText(snapshot_.core_box_underlying_dns);
    ui->enable_fakeip->setChecked(snapshot_.fake_dns);
    S_LOAD_BOOL(fakeip_disable_ipv6)
    ui->fakeip_disable_ipv6->setEnabled(snapshot_.fake_dns);
    connect(ui->enable_fakeip, &QCheckBox::toggled, ui->fakeip_disable_ipv6, &QCheckBox::setEnabled);

    dns_advanced = {
        snapshot_.dns_cache_capacity,
        snapshot_.dns_disable_cache,
        snapshot_.dns_disable_expire,
        snapshot_.dns_persist_cache,
        snapshot_.dns_reverse_mapping,
        snapshot_.dns_optimistic,
        snapshot_.dns_optimistic_timeout,
        snapshot_.dns_query_timeout,
    };
    connect(ui->dns_advanced, &QPushButton::clicked, this, &DialogManageRoutes::show_dns_advanced_editor);

    dns_object_text = snapshot_.dns_object;
    connect(ui->dns_object_edit, &QPushButton::clicked, this, &DialogManageRoutes::show_dns_object_editor);
    connect(ui->use_dns_object, &QCheckBox::stateChanged, this, [=,this](int state) {
        auto useDNSObject = state == Qt::Checked;
        ui->simple_dns_box->setDisabled(useDNSObject);
        ui->dns_resolution_box->setDisabled(useDNSObject);
        ui->dns_advanced->setDisabled(useDNSObject);
        ui->dns_object_edit->setDisabled(!useDNSObject);
    });
    ui->use_dns_object->stateChanged(Qt::Unchecked);
    ui->ruleset_mirror->setCurrentIndex(snapshot_.ruleset_mirror);
    S_LOAD_INT_ENABLE(route_auto_update, route_auto_update_enable)
    S_LOAD_COMBO_STRING(default_domain_strategy)
    ui->domainStrategyCombo->setCurrentText(snapshot_.resolve_domain_strategy);
    S_LOAD_BOOL(use_dns_object)
    ui->remote_dns->setPresets(DnsServerHistory::RemotePresets());
    ui->remote_dns->setRecent(snapshot_.remote_dns_history);
    ui->remote_dns->setServers(snapshot_.remote_dns);
    S_LOAD_BOOL(remote_dns_disable_ipv6)
    ui->direct_dns->setPresets(DnsServerHistory::DirectPresets());
    ui->direct_dns->setRecent(snapshot_.direct_dns_history);
    ui->direct_dns->setPrimaryNote(true);
    ui->direct_dns->setServers(snapshot_.direct_dns);
    S_LOAD_BOOL(direct_dns_disable_ipv6)
    // A top-aligned label beside a tall field: 7/4 of its own height centers its text on the first row.
    for (auto *label : {ui->label_9, ui->label_8}) {
        label->ensurePolished();
        label->setMinimumHeight(label->sizeHint().height() * 7 / 4);
    }
    S_LOAD_COMBO_STRING(dns_final_out)
    S_LOAD_BOOL(enable_dns_routing)
    ui->respect_hosts->setChecked(snapshot_.dns_use_hosts);
    predefined_dns_enabled = snapshot_.dns_predefined_enable;
    predefined_dns_text = snapshot_.dns_predefined_rules.join("\n");
    connect(ui->predefined_dns, &QPushButton::clicked, this, &DialogManageRoutes::show_predefined_dns_editor);
    ui->route_profiles->setItemDelegate(new ManageRoutesRadioDelegate(ui->route_profiles));
    reloadProfileItems();

    connect(ui->route_profiles, &QListWidget::itemDoubleClicked, this, [=,this](const QListWidgetItem* item){
        on_edit_route_clicked();
    });

    connect(ui->route_profiles, &QListWidget::itemChanged, this, &DialogManageRoutes::syncRouteChecks);

    deleteShortcut = new QShortcut(QKeySequence(Qt::Key_Delete), this);

    connect(deleteShortcut, &QShortcut::activated, this, [=,this]{
        on_delete_route_clicked();
    });

    // Scoped to the list, or these would hijack copy/paste in the dialog's text fields.
    auto exportShortcut = new QShortcut(QKeySequence::Copy, ui->route_profiles);
    exportShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(exportShortcut, &QShortcut::activated, this, [=,this]{
        on_export_route_clicked();
    });

    auto importShortcut = new QShortcut(QKeySequence::Paste, ui->route_profiles);
    importShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(importShortcut, &QShortcut::activated, this, [=,this]{
        on_import_route_clicked();
    });

    exitButtons = new QButtonGroup(this);
    exitButtons->addButton(ui->exit_server, static_cast<int>(Configs::ExitThrough::Server));
    exitButtons->addButton(ui->exit_warp, static_cast<int>(Configs::ExitThrough::Warp));
    if (auto *current = exitButtons->button(static_cast<int>(Configs::ExitOf(snapshot_.exit_through)))) current->setChecked(true);
    // Lines each hint up with its radio button's text.
    const int hintIndent = style()->pixelMetric(QStyle::PM_ExclusiveIndicatorWidth, nullptr, ui->exit_server)
                         + style()->pixelMetric(QStyle::PM_RadioButtonLabelSpacing, nullptr, ui->exit_server);
    for (auto *hint : {ui->exit_server_hint, ui->exit_warp_hint}) hint->setIndent(hintIndent);
    ui->warp_mode->setCurrentIndex(snapshot_.warp_mode == "masque" ? 1 : snapshot_.warp_mode == "amnezia" ? 2 : 0);
    S_LOAD_STRING(warp_ep)
    ui->warp_ifc_addrs->setText(snapshot_.warp_ifc_addrs.join(","));
    S_LOAD_STRING(warp_masque_ep)
    ui->warp_masque_ifc_addrs->setText(snapshot_.warp_masque_ifc_addrs.join(","));
    warp_advanced = {
        snapshot_.warp_private_key,
        snapshot_.warp_public_key,
        snapshot_.warp_reserved.join(","),
        snapshot_.warp_masque_private_key,
        snapshot_.warp_masque_peer_public_key,
        snapshot_.warp_masque_sni,
        std::clamp(snapshot_.warp_masque_http_mode, 0, 2),
    };
    warpAmneziaPackets = snapshot_.warp_amnezia_packets;
    // Each mode's fields share one grid cell and its label, so the label follows the shown field.
    auto showWarpMode = [this](int index) {
        const bool masque = index == 1;
        ui->warp_ep->setHidden(masque);
        ui->warp_ifc_addrs->setHidden(masque);
        ui->warp_masque_ep->setHidden(!masque);
        ui->warp_masque_ifc_addrs->setHidden(!masque);
        ui->warp_ep_l->setBuddy(masque ? ui->warp_masque_ep : ui->warp_ep);
        ui->warp_ifc_addrs_l->setBuddy(masque ? ui->warp_masque_ifc_addrs : ui->warp_ifc_addrs);
        if (index == 2 && warpAmneziaPackets.isEmpty()) generate_amnezia_packets();
    };
    showWarpMode(ui->warp_mode->currentIndex());
    connect(ui->warp_mode, &QComboBox::currentIndexChanged, this, showWarpMode);
    connect(ui->warp_autogen, &QPushButton::clicked, this, &DialogManageRoutes::generate_warp_config);
    connect(ui->warp_advanced_btn, &QPushButton::clicked, this, &DialogManageRoutes::show_warp_advanced_editor);

    ui->remote_dns->bindTo(form, ui->label_9->text());
    ui->direct_dns->bindTo(form, ui->label_8->text());
    form->bind(ui->local_override, manageRoutesLocalDnsProblem).label(ui->label_2->text());
    form->bind(ui->route_auto_update, Validation::Rules::Int(30, 525600, Validation::Empty::Required))
        .when([this] { return ui->route_auto_update_enable->isChecked(); });
    connect(ui->route_auto_update_enable, &QAbstractButton::toggled, form, [this] { form->refresh(); });
    // buildDNSSection would emit an unparsable object as "dns": {}, silently dropping every DNS setting.
    form->addRule({ui->use_dns_object}, [this] {
        if (!ui->use_dns_object->isChecked()) return QString();
        if (dns_object_text.trimmed().isEmpty())
            return tr("It is on but empty: click Edit DNS Object to write one, or turn it off.");
        if (auto problem = Validation::JsonObject(dns_object_text); !problem.isEmpty()) return problem;
        if (QString2QJsonObject(dns_object_text).isEmpty()) return tr("The object is empty: add at least its \"servers\", or turn it off.");
        return QString();
    }, nullptr, Validation::Severity::Error, tr("DNS Object"));

    const auto warpOn = [this] { return ui->exit_warp->isChecked(); };
    const auto addresses = Validation::Rules::List(Validation::Split::Comma, Validation::Rules::Cidr(Validation::Family::Any, true));
    form->bind(ui->warp_ep, manageRoutesWarpField(Validation::Rules::HostPort(false))).when(warpOn);
    form->bind(ui->warp_ifc_addrs, manageRoutesWarpField(addresses)).when(warpOn);
    form->bind(ui->warp_masque_ep, manageRoutesWarpField(Validation::Rules::HostPort(false))).when(warpOn);
    form->bind(ui->warp_masque_ifc_addrs, manageRoutesWarpField(addresses)).when(warpOn);
    // Rechecked here so a bad stored value blocks OK even if WARP Advanced is never opened.
    const auto warpAdvancedRule = [this, warpOn](bool masque, const QString &label, Validation::Rule rule, QString WarpAdvancedDraft::*value) {
        form->addRule({ui->warp_advanced_btn}, [this, warpOn, masque, rule = std::move(rule), value] {
            if (!warpOn() || (ui->warp_mode->currentIndex() == 1) != masque) return QString();
            return rule((warp_advanced.*value).trimmed());
        }, nullptr, Validation::Severity::Error, tr("%1 in Advanced").arg(label));
    };
    const auto warpKey = manageRoutesWarpField(Validation::Rules::Base64Key(32));
    warpAdvancedRule(false, tr("Private Key"), warpKey, &WarpAdvancedDraft::private_key);
    warpAdvancedRule(false, tr("Public Key"), warpKey, &WarpAdvancedDraft::public_key);
    warpAdvancedRule(false, tr("Reserved"), manageRoutesReservedProblem, &WarpAdvancedDraft::reserved);
    warpAdvancedRule(true, tr("Private Key"), manageRoutesWarpField({}), &WarpAdvancedDraft::masque_private_key);
    warpAdvancedRule(true, tr("Peer Public Key"), manageRoutesWarpField({}), &WarpAdvancedDraft::masque_peer_public_key);
    const QList<QWidget *> warpFields = {ui->warp_ep, ui->warp_ifc_addrs, ui->warp_masque_ep, ui->warp_masque_ifc_addrs, ui->warp_advanced_btn};
    const auto revalidateWarp = [this, warpFields] {
        for (auto *field : warpFields) form->revalidate(field);
    };
    connect(ui->exit_warp, &QAbstractButton::toggled, form, revalidateWarp);
    connect(ui->warp_mode, &QComboBox::currentIndexChanged, form, revalidateWarp);
    form->addRule({ui->exit_warp, ui->warp_mode}, [this] {
        if (!ui->exit_warp->isChecked() || ui->warp_mode->currentIndex() != 2 || !warpAmneziaPackets.isEmpty()) return QString();
        if (warpAmneziaPending) return tr("Generating the AmneziaWG QUIC signature...");
        return warpAmneziaError.isEmpty() ? tr("AmneziaWG has no QUIC signature yet: click Generate WARP config.") : warpAmneziaError;
    }, nullptr, Validation::Severity::Error, ui->warp_mode_l->text());
    form->markAll();

    ADD_ASTERISK(this)
    // The size it was last closed at, which the layout still grows to its minimum; else the .ui geometry, grown once a translation outgrows it.
    const QStringList savedSize = snapshot_.routing_settings_size.split('x');
    QSize initialSize = savedSize.size() == 2 ? QSize(savedSize[0].toInt(), savedSize[1].toInt()) : QSize();
    if (initialSize.isEmpty()) initialSize = sizeHint().expandedTo(size());
    resize(initialSize.boundedTo(screen()->availableGeometry().size()));
    connect(this, &QDialog::finished, this, [this] {
        if (windowState() & (Qt::WindowMaximized | Qt::WindowFullScreen)) return;
        const auto &settings = Configs::dataManager->settingsRepo;
        settings->routing_settings_size = QStringLiteral("%1x%2").arg(width()).arg(height());
        settings->Save();
    });
}

void DialogManageRoutes::generate_warp_config() {
    if (!Configs_network::ConfirmWarpTerms(this)) return;

    const bool masque = ui->warp_mode->currentIndex() == 1;
    const bool amnezia = ui->warp_mode->currentIndex() == 2;
    const auto originalText = ui->warp_autogen->text();
    ui->warp_autogen->setEnabled(false);
    ui->warp_autogen->setText(tr("Generating config..."));

    QPointer<DialogManageRoutes> self(this);
    runOnNewThread([self, masque, amnezia, originalText] {
        QString error;
        QString packetError;
        const auto conf = Configs_network::RegisterWarp(masque ? "masque" : "wireguard", &error);
        const auto packets = conf && amnezia ? Configs_network::GenerateWarpCamouflage(packetError) : QStringList();
        runOnUiThread([self, masque, amnezia, originalText, conf, error, packets, packetError] {
            if (self == nullptr) return;
            auto *dialog = self.data();
            if (!conf) {
                dialog->ui->warp_autogen->setText(originalText);
                dialog->ui->warp_autogen->setEnabled(true);
                MessageBoxWarning(tr("Failed to generate WARP config"), error);
                return;
            }
            if (amnezia) {
                if (packetError.isEmpty()) dialog->warpAmneziaPackets = packets;
                dialog->warpAmneziaError = packetError;
            }
            QStringList addrs;
            if (!conf->ipv4.isEmpty()) addrs << conf->ipv4 + "/32";
            if (!conf->ipv6.isEmpty()) addrs << conf->ipv6 + "/128";
            if (masque) {
                dialog->warp_advanced.masque_private_key = conf->privateKey;
                dialog->warp_advanced.masque_peer_public_key = conf->peerPublicKey;
                dialog->ui->warp_masque_ep->setText(conf->endpoint);
                dialog->ui->warp_masque_ifc_addrs->setText(addrs.join(","));
            } else {
                dialog->warp_advanced.private_key = conf->privateKey;
                dialog->warp_advanced.public_key = conf->peerPublicKey;
                dialog->warp_advanced.reserved = QListInt2QListString(conf->reserved).join(",");
                dialog->ui->warp_ep->setText(conf->endpoint);
                dialog->ui->warp_ifc_addrs->setText(addrs.join(","));
            }
            dialog->form->refresh();
            if (!packetError.isEmpty()) {
                dialog->ui->warp_autogen->setText(originalText);
                dialog->ui->warp_autogen->setEnabled(true);
                MessageBoxWarning(tr("Failed to generate WARP config"), packetError);
                return;
            }
            dialog->ui->warp_autogen->setText(tr("Success!"));
            setTimeout([dialog, originalText] {
                dialog->ui->warp_autogen->setText(originalText);
                dialog->ui->warp_autogen->setEnabled(true);
            }, dialog, 2000);
        });
    });
}

void DialogManageRoutes::generate_amnezia_packets() {
    if (warpAmneziaPending) return;
    warpAmneziaPending = true;
    QPointer<DialogManageRoutes> self(this);
    runOnNewThread([self] {
        QString error;
        const auto packets = Configs_network::GenerateWarpCamouflage(error);
        runOnUiThread([self, packets, error] {
            if (self == nullptr) return;
            auto *dialog = self.data();
            dialog->warpAmneziaPending = false;
            if (error.isEmpty()) dialog->warpAmneziaPackets = packets;
            dialog->warpAmneziaError = error;
            dialog->form->refresh();
        });
    });
}

DialogManageRoutes::~DialogManageRoutes() {
    if (routeUpdateCancel) *routeUpdateCancel = true;
    delete ui;
}

void DialogManageRoutes::accept() {
    if (!form->validateAll()) return;
    if (chainList.empty()) {
        MessageBoxInfo(tr("Invalid settings"), tr("Routing profile cannot be empty"));
        return;
    }

    Configs::SettingsValues proposed = snapshot_;
    proposed.ruleset_mirror = ui->ruleset_mirror->currentIndex();
    S_SAVE_INT_ENABLE(route_auto_update, route_auto_update_enable)
    proposed.resolve_domain_strategy = ui->domainStrategyCombo->currentText();
    S_SAVE_COMBO_STRING(default_domain_strategy)
    S_SAVE_BOOL(use_dns_object)
    proposed.dns_object = dns_object_text;
    const auto remoteDns = ui->remote_dns->servers();
    const auto directDns = ui->direct_dns->servers();
    if (remoteDns != snapshot_.remote_dns)
        DnsServerHistory::Remember(proposed.remote_dns_history, DnsServerHistory::RemotePresets(), remoteDns);
    if (directDns != snapshot_.direct_dns)
        DnsServerHistory::Remember(proposed.direct_dns_history, DnsServerHistory::DirectPresets(), directDns);
    proposed.remote_dns = remoteDns;
    S_SAVE_BOOL(remote_dns_disable_ipv6)
    proposed.dns_cache_capacity = dns_advanced.cache_capacity;
    proposed.dns_disable_cache = dns_advanced.disable_cache;
    proposed.dns_disable_expire = dns_advanced.disable_expire;
    proposed.dns_persist_cache = dns_advanced.persist_cache;
    proposed.dns_reverse_mapping = dns_advanced.reverse_mapping;
    proposed.dns_optimistic = dns_advanced.optimistic;
    proposed.dns_optimistic_timeout = dns_advanced.optimistic_timeout;
    proposed.dns_query_timeout = dns_advanced.query_timeout;
    proposed.direct_dns = directDns;
    S_SAVE_BOOL(direct_dns_disable_ipv6)
    proposed.core_box_underlying_dns = ui->local_override->text().trimmed();
    S_SAVE_COMBO_STRING(dns_final_out)
    proposed.fake_dns = ui->enable_fakeip->isChecked();
    S_SAVE_BOOL(fakeip_disable_ipv6)
    S_SAVE_BOOL(enable_dns_routing)
    proposed.dns_use_hosts = ui->respect_hosts->isChecked();
    proposed.dns_predefined_enable = predefined_dns_enabled;
    QStringList predefinedRules;
    for (const auto &line : predefined_dns_text.split("\n")) {
        if (!line.trimmed().isEmpty()) predefinedRules.append(line.trimmed());
    }
    proposed.dns_predefined_rules = predefinedRules;

    if (const int chosenExit = exitButtons->checkedId(); chosenExit >= 0)
        proposed.exit_through = Configs::ExitValue(static_cast<Configs::ExitThrough>(chosenExit));
    proposed.warp_ep = ui->warp_ep->text().trimmed();
    proposed.warp_ifc_addrs = manageRoutesWithPrefix(SplitAndTrim(ui->warp_ifc_addrs->text(), ",", false));
    proposed.warp_private_key = warp_advanced.private_key.trimmed();
    proposed.warp_public_key = warp_advanced.public_key.trimmed();
    proposed.warp_reserved = SplitAndTrim(warp_advanced.reserved, ",", false);
    proposed.warp_mode = ui->warp_mode->currentIndex() == 1 ? "masque"
        : ui->warp_mode->currentIndex() == 2 ? "amnezia" : "wireguard";
    proposed.warp_amnezia_packets = warpAmneziaPackets;
    proposed.warp_masque_ep = ui->warp_masque_ep->text().trimmed();
    proposed.warp_masque_private_key = warp_advanced.masque_private_key.trimmed();
    proposed.warp_masque_peer_public_key = warp_advanced.masque_peer_public_key.trimmed();
    proposed.warp_masque_ifc_addrs = manageRoutesWithPrefix(SplitAndTrim(ui->warp_masque_ifc_addrs->text(), ",", false));
    proposed.warp_masque_sni = warp_advanced.masque_sni.trimmed();
    proposed.warp_masque_http_mode = warp_advanced.masque_http_mode;

    QList<std::shared_ptr<Configs::RouteProfile>> changed;
    for (const auto &route : chainList) {
        if (route->id < 0 || editedIds.contains(route->id)) changed << route;
    }
    if (const auto error = Engine::RouteService::Commit(changed, removedIds)) {
        MessageBoxWarning(windowTitle(), error->message);
        return;
    }
    editedIds.clear();
    removedIds.clear();
    // After Commit: the selected profile may be a new one, which only now has an id.
    proposed.current_route_id = currentRoute->id;
    if (const auto error = Engine::SettingsService::ApplyEdit(snapshot_, proposed, {.interactive = true})) {
        MessageBoxWarning(windowTitle(), SettingsApplyErrorText(*error));
        return;
    }

    QDialog::accept();
}

void DialogManageRoutes::on_new_route_clicked() {
    QMenu menu(this);
    menu.addAction(tr("Structured profile"));
    auto* rawAct = menu.addAction(tr("Raw profile"));
    auto* remoteAct = menu.addAction(tr("Remote profile"));
    auto* chosen = menu.exec(ui->new_route->mapToGlobal(QPoint(0, ui->new_route->height())));
    if (chosen == nullptr) return;

    auto newProfile = Configs::dataManager->routesRepo->NewRouteProfile();
    auto onCreated = [=, this](const std::shared_ptr<Configs::RouteProfile>& chain) {
        chainList << chain;
        // Saved at once: Cancel here must not undo the editor's Save.
        if (const auto error = Engine::RouteService::Commit({chain}, {})) MessageBoxWarning(windowTitle(), error->message);
        reloadProfileItems();
    };
    if (chosen == rawAct) {
        newProfile->isRaw = true;
        auto* rawWidget = new RawRouteItem(this, newProfile);
        rawWidget->setAttribute(Qt::WA_DeleteOnClose);
        rawWidget->setWindowModality(Qt::ApplicationModal);
        rawWidget->show();
        connect(rawWidget, &RawRouteItem::settingsChanged, this, onCreated);
    } else {
        // Remote profiles are structured underneath; RouteItem grows a "Remote source" section when isRemote.
        if (chosen == remoteAct) newProfile->isRemote = true;
        routeChainWidget = new RouteItem(this, newProfile);
        routeChainWidget->setAttribute(Qt::WA_DeleteOnClose);
        routeChainWidget->setWindowModality(Qt::ApplicationModal);
        routeChainWidget->show();
        connect(routeChainWidget, &RouteItem::settingsChanged, this, onCreated);
    }
}

void DialogManageRoutes::on_export_route_clicked()
{
    auto idx = ui->route_profiles->currentRow();
    if (idx < 0) return;

    QString warnings;
    QApplication::clipboard()->setText(chainList[idx]->ToShareLink(&warnings));
    if (!warnings.isEmpty()) manageRoutesPlainBox(this, QMessageBox::Information, tr("Exported with warnings"), warnings.trimmed());

    QToolTip::showText(QCursor::pos(), tr("Copied!"), this);
    int r = ++tooltipID;
    QTimer::singleShot(1500, [=,this] {
        if (tooltipID != r) return;
        QToolTip::hideText();
    });
}

void DialogManageRoutes::applyImportedProfile(const std::shared_ptr<Configs::RouteProfile>& profile, bool wasOldArray)
{
    if (wasOldArray) {
        // A legacy rule array carries no name / default outbound, so it cannot be saved unedited.
        auto shell = Configs::dataManager->routesRepo->NewRouteProfile();
        shell->Rules = profile->Rules;
        routeChainWidget = new RouteItem(this, shell);
        routeChainWidget->setAttribute(Qt::WA_DeleteOnClose);
        routeChainWidget->setWindowModality(Qt::ApplicationModal);
        routeChainWidget->show();
        connect(routeChainWidget, &RouteItem::settingsChanged, this, [=, this](const std::shared_ptr<Configs::RouteProfile>& chain) {
            chainList << chain;
            // Saved at once: Cancel here must not undo the editor's Save.
            if (const auto error = Engine::RouteService::Commit({chain}, {})) MessageBoxWarning(windowTitle(), error->message);
            reloadProfileItems();
        });
    } else {
        chainList << profile;
        currentRoute = profile;
        reloadProfileItems();
    }
}

bool DialogManageRoutes::tryImportRemoteRoutesLink(const QString& text)
{
    bool wasRemoteRouteLink = false;
    QString error;
    auto profiles = Configs::RouteProfile::FromRemoteRoutesLink(text, &wasRemoteRouteLink, &error);
    if (!wasRemoteRouteLink) return false;

    if (profiles.isEmpty()) {
        MessageBoxWarning(tr("Add remote routing profiles"),
                          error.isEmpty() ? tr("No valid remote routing profiles in the link.") : error);
        return true;
    }

    QString prompt = tr("Add these remote routing profiles?") + "\n";
    for (int i = 0; i < profiles.size(); ++i) {
        prompt += QString("\n%1. %2  (%3: %4)")
                      .arg(i + 1)
                      .arg(profiles[i]->remoteURL, tr("auto update"), profiles[i]->autoUpdate ? tr("On") : tr("Off"));
    }
    QMessageBox box(QMessageBox::Question, tr("Add remote routing profiles"), prompt, QMessageBox::Yes | QMessageBox::No, this);
    box.setTextFormat(Qt::PlainText);
    if (box.exec() != QMessageBox::Yes) {
        return true; // it was a remoteRoute link; the user declined
    }

    for (const auto& p : profiles) chainList << p;
    reloadProfileItems();
    updateRemoteProfiles(profiles);
    return true;
}

void DialogManageRoutes::on_import_route_clicked()
{
    const QString clip = QApplication::clipboard()->text().trimmed();
    if (tryImportRemoteRoutesLink(clip)) return;
    if (!clip.isEmpty()) {
        QString fatal, warnings;
        bool wasOldArray = false;
        int newEndpoints = 0;
        if (auto profile = Configs::RouteProfile::FromShareInput(clip, &fatal, &warnings, &wasOldArray, false, &newEndpoints)) {
            if (newEndpoints > 0 && !manageRoutesEndpointTargetAccepted(tr("Import from clipboard"))) return;
            const QString what = wasOldArray ? tr("a routing rule list")
                                             : tr("routing profile \"%1\"").arg(profile->name);
            QString prompt = tr("Import %1 from the clipboard?").arg(what);
            if (const auto dns = profile->ActiveDns()) prompt += "\n\n" + manageRoutesDnsNote(*dns);
            if (newEndpoints > 0) prompt += "\n\n" + manageRoutesEndpointsNote(newEndpoints);
            if (!warnings.isEmpty()) prompt += "\n\n" + tr("Note:") + "\n" + warnings.trimmed();
            QMessageBox box(QMessageBox::Question, tr("Import from clipboard"), prompt,
                            QMessageBox::Yes | QMessageBox::No, this);
            box.setTextFormat(Qt::PlainText);
            if (box.exec() == QMessageBox::Yes) {
                if (newEndpoints > 0) {
                    profile = Configs::RouteProfile::FromShareInput(clip, &fatal, &warnings, &wasOldArray);
                    manageRoutesRefreshProxyList();
                }
                if (profile != nullptr) applyImportedProfile(profile, wasOldArray);
                return;
            }
        }
    }

    auto w = new QDialog(this);
    w->setWindowTitle(tr("Import Routing Profile"));
    w->setWindowModality(Qt::ApplicationModal);

    auto layout = new QGridLayout(w);
    auto tEdit = new QTextEdit(w);
    tEdit->setPlaceholderText(tr("Paste a Throne route link, a remoteRoute link, a base64 blob, or a JSON rule array"));
    layout->addWidget(tEdit, 0, 0);

    auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, w);
    layout->addWidget(buttons, 1, 0);

    connect(buttons, &QDialogButtonBox::accepted, w, [=, this] {
        if (tryImportRemoteRoutesLink(tEdit->toPlainText())) { w->accept(); return; }
        QString fatal, warnings;
        bool wasOldArray = false;
        int newEndpoints = 0;
        auto profile = Configs::RouteProfile::FromShareInput(tEdit->toPlainText(), &fatal, &warnings, &wasOldArray, false, &newEndpoints);
        if (!profile) {
            MessageBoxWarning(tr("Invalid input"), tr("Could not import this routing profile:\n") + fatal);
            return;
        }
        if (newEndpoints > 0) {
            if (!manageRoutesEndpointTargetAccepted(tr("Import routing profile"))) return;
            warnings.clear();
            profile = Configs::RouteProfile::FromShareInput(tEdit->toPlainText(), &fatal, &warnings, &wasOldArray);
            manageRoutesRefreshProxyList();
            if (profile == nullptr) return;
        }
        if (const auto dns = profile->ActiveDns()) warnings.prepend(manageRoutesDnsNote(*dns) + "\n");
        if (!warnings.isEmpty()) manageRoutesPlainBox(w, QMessageBox::Information, tr("Imported with warnings"), warnings.trimmed());
        applyImportedProfile(profile, wasOldArray);
        w->accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, w, &QDialog::reject);

    w->exec();
    w->deleteLater();
}

void DialogManageRoutes::on_clone_route_clicked() {
    auto idx = ui->route_profiles->currentRow();
    if (idx < 0) return;

    auto chainCopy = std::make_shared<Configs::RouteProfile>(*chainList[idx]);
    chainCopy->name = chainCopy->name + " clone";
    chainCopy->id = -1;
    // The clone is the user's own: the subscription keeps updating only the original.
    chainCopy->source_gid = -1;
    chainList.append(chainCopy);
    reloadProfileItems();
}

void DialogManageRoutes::on_edit_route_clicked() {
    auto idx = ui->route_profiles->currentRow();
    if (idx < 0) return;

    auto onEdited = [=, this](const std::shared_ptr<Configs::RouteProfile>& chain) {
        if (currentRoute == chainList[idx]) currentRoute = chain;
        chainList[idx] = chain;
        // Saved at once: Cancel here must not undo the editor's Save.
        if (const auto error = Engine::RouteService::Commit({chain}, {})) {
            editedIds.insert(chain->id);
            MessageBoxWarning(windowTitle(), error->message);
        } else {
            editedIds.remove(chain->id);
        }
        reloadProfileItems();
    };

    if (chainList[idx]->isRaw) {
        auto* rawWidget = new RawRouteItem(this, chainList[idx]);
        rawWidget->setAttribute(Qt::WA_DeleteOnClose);
        rawWidget->setWindowModality(Qt::ApplicationModal);
        rawWidget->show();
        connect(rawWidget, &RawRouteItem::settingsChanged, this, onEdited);
    } else {
        routeChainWidget = new RouteItem(this, chainList[idx]);
        routeChainWidget->setAttribute(Qt::WA_DeleteOnClose);
        routeChainWidget->setWindowModality(Qt::ApplicationModal);
        routeChainWidget->show();
        connect(routeChainWidget, &RouteItem::settingsChanged, this, onEdited);
    }
}

void DialogManageRoutes::on_delete_route_clicked() {
    auto idx = ui->route_profiles->currentRow();
    if (idx < 0) return;
    if (chainList.size() == 1) {
        MessageBoxWarning(tr("Invalid operation"), tr("Routing Profiles cannot be empty, try adding another profile or editing this one"));
        return;
    }

    auto profileToDel = chainList[idx];
    chainList.removeAt(idx);
    if (profileToDel->id >= 0) removedIds << profileToDel->id;
    if (profileToDel == currentRoute) {
        currentRoute = chainList[0];
    }
    reloadProfileItems();
}

void DialogManageRoutes::on_update_route_clicked() {
    if (routeUpdateRunning) {
        QMenu menu(this);
        auto* cancelAct = menu.addAction(tr("Cancel"));
        auto* chosen = menu.exec(ui->update_route->mapToGlobal(QPoint(0, ui->update_route->height())));
        // Guard against the batch having finished while the menu was open.
        if (chosen == cancelAct && routeUpdateRunning) {
            *routeUpdateCancel = true;
            ui->update_route->setText(tr("Cancelling..."));
        }
        return;
    }

    const int idx = ui->route_profiles->currentRow();
    const bool selIsRemote = idx >= 0 && chainList[idx]->isRemote;

    QMenu menu(this);
    QAction* updateSelAct = selIsRemote ? menu.addAction(tr("Update selected")) : nullptr;
    auto* updateAllAct = menu.addAction(tr("Update all"));
    auto* chosen = menu.exec(ui->update_route->mapToGlobal(QPoint(0, ui->update_route->height())));
    if (chosen == nullptr) return;

    if (chosen == updateSelAct) {
        updateRemoteProfiles({chainList[idx]});
        return;
    }

    if (chosen == updateAllAct) {
        QList<std::shared_ptr<Configs::RouteProfile>> remotes;
        for (const auto& p : chainList) {
            if (p->isRemote && !p->remoteURL.trimmed().isEmpty()) remotes << p;
        }
        if (remotes.isEmpty()) {
            MessageBoxInfo(tr("No remote profiles"), tr("There are no remote routing profiles to update."));
            return;
        }
        updateRemoteProfiles(remotes);
    }
}

void DialogManageRoutes::updateRemoteProfiles(const QList<std::shared_ptr<Configs::RouteProfile>>& profiles) {
    if (routeUpdateRunning || profiles.isEmpty()) return;
    routeUpdateRunning = true;
    const auto cancel = std::make_shared<std::atomic<bool>>(false);
    routeUpdateCancel = cancel;
    const int total = profiles.size();
    // The entries may be the live routes a config build reads, so the worker updates copies.
    QList<std::shared_ptr<Configs::RouteProfile>> copies;
    for (const auto& p : profiles) copies << std::make_shared<Configs::RouteProfile>(*p);

    // The button stays enabled during the run so its click can offer Cancel (see the slot above).
    auto progressText = [total](int current) {
        return total <= 1 ? tr("Updating...") : tr("Updating (%1 / %2)").arg(current).arg(total);
    };
    ui->update_route->setText(progressText(1));

    // Closing the window deletes it mid-run: the worker sees the cancel flag, the UI callbacks the null pointer.
    QPointer<DialogManageRoutes> self(this);
    runOnNewThread([=, this] {
        QStringList failures;
        QStringList warned;
        QList<int> updated;
        for (int i = 0; i < copies.size(); ++i) {
            if (cancel->load()) break;
            const int current = i + 1;
            runOnUiThread([=, this] {
                if (self && routeUpdateRunning && !cancel->load())
                    ui->update_route->setText(progressText(current));
            });
            QString warnings;
            const QString err = RouteUpdate::UpdateProfile(copies[i], &warnings);
            if (err.isEmpty()) updated << i;
            else failures << (copies[i]->name + ": " + err);
            if (err.isEmpty() && !warnings.trimmed().isEmpty()) warned << (copies[i]->name + ":\n" + warnings.trimmed());
        }
        const int ok = static_cast<int>(updated.size());
        const bool cancelled = cancel->load();
        runOnUiThread([=, this] {
            if (!self) return;
            routeUpdateRunning = false;
            ui->update_route->setText(tr("Update"));
            for (const int i : updated) {
                // Gone once edited or deleted meanwhile: that edit wins.
                const auto index = chainList.indexOf(profiles[i]);
                if (index < 0) continue;
                if (currentRoute == profiles[i]) currentRoute = copies[i];
                chainList[index] = copies[i];
                editedIds.insert(copies[i]->id);
            }
            reloadProfileItems();
            const QString details = warned.join("\n\n");
            const QString note = warned.isEmpty() ? QString()
                                                  : "\n\n" + tr("%n profile(s) were updated with warnings; see the details.", "", static_cast<int>(warned.size()));
            if (cancelled) {
                manageRoutesPlainBox(this, QMessageBox::Information, tr("Update cancelled"),
                                     tr("Cancelled: updated %1 of %2, %3 failed.").arg(ok).arg(total).arg(failures.size()) + note, details);
            } else if (failures.isEmpty()) {
                manageRoutesPlainBox(this, QMessageBox::Information, tr("Update complete"),
                                     tr("Updated %1 remote routing profile(s).").arg(ok) + note, details);
            } else {
                manageRoutesPlainBox(this, QMessageBox::Warning, tr("Update finished with errors"),
                                     tr("Updated %1, failed %2:\n%3").arg(ok).arg(failures.size()).arg(failures.join("\n")) + note, details);
            }
        });
    });
}
