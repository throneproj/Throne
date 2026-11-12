#include "include/ui/widget/MuxSettingsWidget.h"

#include "include/ui/utils/GuiUtils.hpp"
#include "include/ui/utils/ValidationForm.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>
#include <QRegularExpressionValidator>
#include <QVBoxLayout>

#include <climits>

namespace {
    QString muxPositiveText(const int value) {
        return value > 0 ? QString::number(value) : QString();
    }

    // Named so the settings search can index and reveal them.
    QLabel *addMuxField(QWidget *owner, QGridLayout *grid, int row, int column, const QString &name, const QString &text, QWidget *field) {
        auto *label = new QLabel(text, owner);
        label->setObjectName(name);
        label->setBuddy(field);
        grid->addWidget(label, row, column);
        grid->addWidget(field, row, column + 1);
        return label;
    }

    QString muxFieldName(const QString &labelPrefix, const QLabel *label) {
        return labelPrefix.isEmpty() ? label->text() : QStringLiteral("%1 %2").arg(labelPrefix, label->text());
    }

    template <typename Editor, typename Settings>
    void openMuxDialog(QWidget *parent, const QString &title, const Settings &settings, std::function<void(const Settings &)> done) {
        auto *dialog = new QDialog(parent);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->setWindowTitle(title);
        auto *layout = new QVBoxLayout(dialog);
        auto *editor = new Editor(dialog);
        editor->setSettings(settings);
        layout->addWidget(editor);
        layout->addStretch(1);
        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dialog);
        layout->addWidget(buttons);

        auto *form = new Validation::Form(dialog);
        form->attachStatus(buttons);
        editor->bindValidation(form);
        QObject::connect(buttons, &QDialogButtonBox::accepted, dialog, [dialog, form, editor, done = std::move(done)] {
            if (!form->validateAll()) return;
            done(editor->settings());
            dialog->accept();
        });
        QObject::connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
        form->markAll();
        dialog->open();
    }
}

MuxSettingsWidget::MuxSettingsWidget(QWidget *parent) : QWidget(parent) {
    auto *grid = new QGridLayout(this);
    grid->setContentsMargins(0, 0, 0, 0);
    const auto addField = [this, grid](int row, int column, const QString &name, const QString &text, QWidget *field) {
        return addMuxField(this, grid, row, column, name, text, field);
    };
    const auto numberEdit = [this] {
        auto *edit = new QLineEdit(this);
        edit->setValidator(QRegExpValidator_Number);
        return edit;
    };

    m_protocol = new QComboBox(this);
    m_protocol->addItems(Configs::muxProtocols);
    addField(0, 0, "mux_protocol_l", tr("Protocol"), m_protocol);
    m_mode = new QComboBox(this);
    m_mode->addItems({tr("Max Streams"), tr("Max Connections")});
    m_modeLabel = addField(0, 2, "mux_mode_l", tr("Mode"), m_mode);
    m_modeLabel->setToolTip(tr("<html><head/><body><p><b>Max Streams</b>: reuse a connection until it carries this many streams, then open another.</p>"
                               "<p><b>Max Connections</b>: keep at most this many connections, and open a new one only while the least busy "
                               "connection already carries Min Streams streams.</p></body></html>"));

    m_maxStreams = numberEdit();
    m_maxStreamsLabel = addField(1, 0, "mux_max_streams_l", tr("Max Streams"), m_maxStreams);
    m_maxConnections = numberEdit();
    m_maxConnectionsLabel = addField(2, 0, "mux_max_connections_l", tr("Max Connections"), m_maxConnections);
    m_minStreams = numberEdit();
    m_minStreamsLabel = addField(2, 2, "mux_min_streams_l", tr("Min Streams"), m_minStreams);

    m_padding = new QCheckBox(tr("Padding"), this);
    m_padding->setObjectName("mux_padding");
    grid->addWidget(m_padding, 3, 0, 1, 4);
    m_brutal = new QCheckBox(tr("TCP Brutal"), this);
    m_brutal->setObjectName("mux_brutal");
    m_brutal->setToolTip(tr("<html><head/><body><p>TCP Brutal congestion control at the speeds below; the server must support it. "
                            "Every stream then shares one connection, so the mode is ignored.</p></body></html>"));
    grid->addWidget(m_brutal, 4, 0, 1, 4);
    m_brutalUp = numberEdit();
    m_brutalUpLabel = addField(5, 0, "mux_brutal_up_l", tr("Upload (Mb/s)"), m_brutalUp);
    m_brutalDown = numberEdit();
    m_brutalDownLabel = addField(5, 2, "mux_brutal_down_l", tr("Download (Mb/s)"), m_brutalDown);
    grid->setColumnStretch(4, 1);

    connect(m_mode, &QComboBox::currentIndexChanged, this, &MuxSettingsWidget::updateControls);
    connect(m_brutal, &QCheckBox::toggled, this, &MuxSettingsWidget::updateControls);
    ADD_ASTERISK(this)
    updateControls();
}

void MuxSettingsWidget::setSettings(const Configs::MuxSettings &settings) {
    m_protocol->setCurrentIndex(qMax(0, m_protocol->findText(settings.protocol)));
    m_mode->setCurrentIndex(settings.by_connections ? 1 : 0);
    m_maxStreams->setText(muxPositiveText(settings.max_streams));
    m_maxConnections->setText(muxPositiveText(settings.max_connections));
    m_minStreams->setText(QString::number(qMax(0, settings.min_streams)));
    m_padding->setChecked(settings.padding);
    m_brutal->setChecked(settings.brutal);
    m_brutalUp->setText(muxPositiveText(settings.brutal_up_mbps));
    m_brutalDown->setText(muxPositiveText(settings.brutal_down_mbps));
    updateControls();
}

Configs::MuxSettings MuxSettingsWidget::settings() const {
    Configs::MuxSettings settings;
    settings.protocol = m_protocol->currentText();
    settings.by_connections = m_mode->currentIndex() == 1;
    settings.max_streams = m_maxStreams->text().trimmed().toInt();
    settings.max_connections = m_maxConnections->text().trimmed().toInt();
    settings.min_streams = m_minStreams->text().trimmed().toInt();
    settings.padding = m_padding->isChecked();
    settings.brutal = m_brutal->isChecked();
    settings.brutal_up_mbps = m_brutalUp->text().trimmed().toInt();
    settings.brutal_down_mbps = m_brutalDown->text().trimmed().toInt();
    return settings;
}

void MuxSettingsWidget::bindValidation(Validation::Form *form, const QString &labelPrefix) {
    using namespace Validation;
    m_form = form;
    const auto name = [&labelPrefix](const QLabel *label) { return muxFieldName(labelPrefix, label); };
    form->bind(m_maxStreams, Rules::Int(1, INT_MAX, Empty::Required)).label(name(m_maxStreamsLabel));
    form->bind(m_maxConnections, Rules::Int(1, INT_MAX, Empty::Required)).label(name(m_maxConnectionsLabel));
    form->bind(m_minStreams, Rules::Int(0, INT_MAX)).label(name(m_minStreamsLabel));
    form->bind(m_brutalUp, Rules::Int(1, INT_MAX, Empty::Required)).label(name(m_brutalUpLabel));
    form->bind(m_brutalDown, Rules::Int(1, INT_MAX, Empty::Required)).label(name(m_brutalDownLabel));
}

void MuxSettingsWidget::updateControls() {
    const bool connections = m_mode->currentIndex() == 1;
    const bool brutal = m_brutal->isChecked();
    for (QWidget *w: std::initializer_list<QWidget *>{m_maxStreamsLabel, m_maxStreams}) {
        w->setVisible(!connections);
    }
    for (QWidget *w: std::initializer_list<QWidget *>{m_maxConnectionsLabel, m_maxConnections, m_minStreamsLabel, m_minStreams}) {
        w->setVisible(connections);
    }
    // With TCP Brutal the core sends every stream over a single connection.
    for (QWidget *w: std::initializer_list<QWidget *>{m_modeLabel, m_mode, m_maxStreamsLabel, m_maxStreams, m_maxConnectionsLabel,
                                                     m_maxConnections, m_minStreamsLabel, m_minStreams}) {
        w->setEnabled(!brutal);
    }
    for (QWidget *w: std::initializer_list<QWidget *>{m_brutalUpLabel, m_brutalUp, m_brutalDownLabel, m_brutalDown}) {
        w->setEnabled(brutal);
    }
    if (m_form != nullptr) m_form->refresh();
}

void MuxSettingsWidget::openDialog(QWidget *parent, const Configs::MuxSettings &settings,
                                   std::function<void(const Configs::MuxSettings &)> done) {
    openMuxDialog<MuxSettingsWidget>(parent, tr("Custom Multiplex"), settings, std::move(done));
}

XrayMuxSettingsWidget::XrayMuxSettingsWidget(QWidget *parent) : QWidget(parent) {
    auto *grid = new QGridLayout(this);
    grid->setContentsMargins(0, 0, 0, 0);
    const auto countEdit = [this](int min) {
        auto *edit = new QLineEdit(this);
        edit->setValidator(new QIntValidator(min, Configs::kXrayMuxMaxCount, edit));
        return edit;
    };

    m_concurrency = countEdit(-1);
    m_concurrencyLabel = addMuxField(this, grid, 0, 0, "xray_mux_concurrency_l", tr("Concurrency"), m_concurrency);
    m_concurrencyLabel->setToolTip(tr("<html><head/><body><p>Streams one connection carries before another opens. "
                                      "0 uses Xray's 8, and -1 sends TCP outside the mux.</p></body></html>"));
    m_maxConnections = countEdit(0);
    m_maxConnections->setPlaceholderText(tr("No limit"));
    m_maxConnectionsLabel = addMuxField(this, grid, 0, 2, "xray_mux_max_connections_l", tr("Max Connections"), m_maxConnections);
    m_maxConnectionsLabel->setToolTip(tr("<html><head/><body><p>Keep at most this many connections, for TCP and for XUDP each. Once every one "
                                         "carries Concurrency streams, new streams join the least busy one.</p></body></html>"));
    m_xudpConcurrency = countEdit(-1);
    m_xudpConcurrencyLabel = addMuxField(this, grid, 1, 0, "xray_mux_xudp_concurrency_l", tr("XUDP Concurrency"), m_xudpConcurrency);
    m_xudpConcurrencyLabel->setToolTip(tr("<html><head/><body><p>UDP streams per connection, on connections of their own. "
                                          "0 carries UDP on the TCP connections, and -1 sends it outside the mux.</p></body></html>"));
    // Same order as Configs::xrayXudpProxyUDP443Modes.
    m_xudpProxyUDP443 = new QComboBox(this);
    m_xudpProxyUDP443->addItems({tr("Reject"), tr("Allow"), tr("Skip")});
    auto *udp443Label = addMuxField(this, grid, 1, 2, "xray_mux_xudp_proxy_udp443_l", tr("UDP/443"), m_xudpProxyUDP443);
    udp443Label->setToolTip(tr("<html><head/><body><p>UDP to port 443, mostly QUIC. <b>Reject</b> makes browsers fall back to TCP (Xray's default), "
                               "<b>Allow</b> carries it through the mux, and <b>Skip</b> sends it outside the mux.</p></body></html>"));
    grid->setColumnStretch(4, 1);
    ADD_ASTERISK(this)
}

void XrayMuxSettingsWidget::setSettings(const Configs::XrayMuxSettings &settings) {
    m_concurrency->setText(QString::number(settings.concurrency));
    m_maxConnections->setText(muxPositiveText(settings.max_connections));
    m_xudpConcurrency->setText(QString::number(settings.xudp_concurrency));
    m_xudpProxyUDP443->setCurrentIndex(qMax(0, Configs::xrayXudpProxyUDP443Modes.indexOf(settings.xudp_proxy_udp443)));
}

Configs::XrayMuxSettings XrayMuxSettingsWidget::settings() const {
    Configs::XrayMuxSettings settings;
    settings.concurrency = m_concurrency->text().trimmed().toInt();
    settings.max_connections = m_maxConnections->text().trimmed().toInt();
    settings.xudp_concurrency = m_xudpConcurrency->text().trimmed().toInt();
    settings.xudp_proxy_udp443 = Configs::xrayXudpProxyUDP443Modes.value(m_xudpProxyUDP443->currentIndex());
    return settings;
}

void XrayMuxSettingsWidget::bindValidation(Validation::Form *form, const QString &labelPrefix) {
    using namespace Validation;
    const auto name = [&labelPrefix](const QLabel *label) { return muxFieldName(labelPrefix, label); };
    form->bind(m_concurrency, Rules::Int(-1, Configs::kXrayMuxMaxCount, Empty::Required)).label(name(m_concurrencyLabel));
    form->bind(m_maxConnections, Rules::Int(0, Configs::kXrayMuxMaxCount)).label(name(m_maxConnectionsLabel));
    form->bind(m_xudpConcurrency, Rules::Int(-1, Configs::kXrayMuxMaxCount, Empty::Required)).label(name(m_xudpConcurrencyLabel));
}

void XrayMuxSettingsWidget::openDialog(QWidget *parent, const Configs::XrayMuxSettings &settings,
                                       std::function<void(const Configs::XrayMuxSettings &)> done) {
    openMuxDialog<XrayMuxSettingsWidget>(parent, tr("Custom Mux"), settings, std::move(done));
}
