#include "include/ui/profile/edit_masterdns.h"

#include "include/configs/common/DnsTunnel.h"
#include "include/global/Utils.hpp"
#include "include/ui/utils/ValidationForm.h"

#include <QRegularExpressionValidator>

#include <climits>

namespace {
    void editMasterDnsSelect(QComboBox *combo, const QString &value) {
        int index = combo->findData(value);
        if (index < 0) {
            combo->addItem(value, value);
            index = combo->count() - 1;
        }
        combo->setCurrentIndex(index);
    }

    QString editMasterDnsNumber(int value) {
        return value > 0 ? QString::number(value) : QString();
    }

    int editMasterDnsInt(const QLineEdit *edit) {
        const auto value = Validation::ToInt(edit->text().trimmed());
        return value && *value >= 0 && *value <= INT_MAX ? int(*value) : 0;
    }

    QString editMasterDnsData(const QComboBox *combo) {
        return combo->currentData().toString();
    }
}

EditMasterDns::EditMasterDns(QWidget *parent) : QWidget(parent), ui(new Ui::EditMasterDns) {
    ui->setupUi(this);

    ui->domains->setVisibleLines(3);
    ui->mode->addItem(QStringLiteral("SOCKS5"), QStringLiteral("socks5"));
    ui->mode->addItem(tr("TCP / custom chain"), QStringLiteral("tcp"));
    for (const auto &method: Configs::masterDnsEncryptionMethods) ui->encryption_method->addItem(method, method);
    ui->resolvers->setFlavor(Configs::ResolverFlavor::MasterDns);
    ui->balancing_strategy->addItem(tr("Default"), QString());
    for (const auto &strategy: Configs::masterDnsBalancingStrategies) ui->balancing_strategy->addItem(strategy, strategy);
    for (auto *combo: {ui->upload_compression, ui->download_compression}) {
        combo->addItem(tr("Default"), QString());
        for (const auto &compression: Configs::masterDnsCompressions) {
            combo->addItem(compression == QLatin1String("off") ? tr("Off") : compression, compression);
        }
    }

    auto *digits = new QRegularExpressionValidator(QRegularExpression("^[0-9]*$"), this);
    for (auto *edit: {ui->packet_duplication, ui->setup_packet_duplication, ui->mtu_min_upload, ui->mtu_max_upload,
                      ui->mtu_min_download, ui->mtu_max_download, ui->mtu_test_retries, ui->mtu_test_parallelism}) {
        edit->setValidator(digits);
    }
}

EditMasterDns::~EditMasterDns() {
    delete ui;
}

QString EditMasterDns::method() const {
    return editMasterDnsData(ui->encryption_method);
}

void EditMasterDns::onStart(std::shared_ptr<Configs::Profile> _ent) {
    this->ent = _ent;
    auto outbound = this->ent->MasterDns();

    ui->domains->setPlainText(outbound->domains.join("\n"));
    editMasterDnsSelect(ui->mode, outbound->mode.isEmpty() ? QStringLiteral("socks5") : outbound->mode);
    editMasterDnsSelect(ui->encryption_method, outbound->encryption_method.isEmpty() ? QStringLiteral("xor") : outbound->encryption_method);
    ui->encryption_key->setText(outbound->encryption_key);
    ui->resolvers->load(outbound->resolvers);
    ui->resolvers->setProfileId(ent->id);
    editMasterDnsSelect(ui->balancing_strategy, outbound->balancing_strategy);

    ui->packet_duplication->setText(editMasterDnsNumber(outbound->packet_duplication));
    ui->setup_packet_duplication->setText(editMasterDnsNumber(outbound->setup_packet_duplication));
    editMasterDnsSelect(ui->upload_compression, outbound->upload_compression);
    editMasterDnsSelect(ui->download_compression, outbound->download_compression);
    ui->base64_response->setChecked(outbound->base64_response);

    ui->mtu_min_upload->setText(editMasterDnsNumber(outbound->mtu_min_upload));
    ui->mtu_max_upload->setText(editMasterDnsNumber(outbound->mtu_max_upload));
    ui->mtu_min_download->setText(editMasterDnsNumber(outbound->mtu_min_download));
    ui->mtu_max_download->setText(editMasterDnsNumber(outbound->mtu_max_download));
    ui->mtu_test_retries->setText(editMasterDnsNumber(outbound->mtu_test_retries));
    ui->mtu_test_timeout->setText(outbound->mtu_test_timeout);
    ui->mtu_test_parallelism->setText(editMasterDnsNumber(outbound->mtu_test_parallelism));
    ui->mtu_auto_remove_low->setChecked(outbound->mtu_auto_remove_low);
}

bool EditMasterDns::onEnd() {
    auto outbound = this->ent->MasterDns();

    outbound->domains = SplitAndTrim(ui->domains->toPlainText(), "\n", false);
    outbound->mode = editMasterDnsData(ui->mode);
    outbound->encryption_method = method();
    outbound->encryption_key = ui->encryption_key->text();
    outbound->resolvers = ui->resolvers->save();
    outbound->balancing_strategy = editMasterDnsData(ui->balancing_strategy);

    outbound->packet_duplication = editMasterDnsInt(ui->packet_duplication);
    outbound->setup_packet_duplication = editMasterDnsInt(ui->setup_packet_duplication);
    outbound->upload_compression = editMasterDnsData(ui->upload_compression);
    outbound->download_compression = editMasterDnsData(ui->download_compression);
    outbound->base64_response = ui->base64_response->isChecked();

    outbound->mtu_min_upload = editMasterDnsInt(ui->mtu_min_upload);
    outbound->mtu_max_upload = editMasterDnsInt(ui->mtu_max_upload);
    outbound->mtu_min_download = editMasterDnsInt(ui->mtu_min_download);
    outbound->mtu_max_download = editMasterDnsInt(ui->mtu_max_download);
    outbound->mtu_test_retries = editMasterDnsInt(ui->mtu_test_retries);
    outbound->mtu_test_timeout = ui->mtu_test_timeout->text().trimmed();
    outbound->mtu_test_parallelism = editMasterDnsInt(ui->mtu_test_parallelism);
    outbound->mtu_auto_remove_low = ui->mtu_auto_remove_low->isChecked();
    return true;
}

void EditMasterDns::bindValidation(Validation::Form &form) {
    using namespace Validation;
    using namespace Configs::Checks;

    form.bindLines(ui->domains, DnsTunnelDomain, this);
    form.addRule({ui->domains}, [this] {
        return Items(ui->domains->toPlainText(), Split::Newline).isEmpty() ? DnsTunnelDomain({}) : QString();
    }, this);
    form.addRule({ui->mode}, [this] {
        return OneOf(editMasterDnsData(ui->mode), Configs::masterDnsModes, Empty::Required);
    }, this);
    form.addRule({ui->encryption_method}, [this] {
        return OneOf(method(), Configs::masterDnsEncryptionMethods, Empty::Required);
    }, this);
    form.addRule({ui->encryption_key, ui->encryption_method}, [this] {
        return MasterDnsEncryptionKey(method(), ui->encryption_key->text());
    }, this);

    ui->resolvers->bindValidation(form, this);
    form.addRule({ui->balancing_strategy}, [this] {
        return OneOf(editMasterDnsData(ui->balancing_strategy), Configs::masterDnsBalancingStrategies);
    }, this);

    form.bind(ui->packet_duplication, Rules::Int(0, 10), this);
    form.bind(ui->setup_packet_duplication, Rules::Int(0, 12), this);
    for (auto *combo: {ui->upload_compression, ui->download_compression}) {
        form.addRule({combo}, [combo] { return OneOf(editMasterDnsData(combo), Configs::masterDnsCompressions); }, this);
    }

    for (auto *edit: {ui->mtu_min_upload, ui->mtu_max_upload, ui->mtu_min_download, ui->mtu_max_download,
                      ui->mtu_test_retries, ui->mtu_test_parallelism}) {
        form.bind(edit, Rules::Int(0, INT_MAX), this);
    }
    form.addRule({ui->mtu_min_upload, ui->mtu_max_upload}, [this] {
        return MasterDnsMtuRange(editMasterDnsInt(ui->mtu_min_upload), editMasterDnsInt(ui->mtu_max_upload), 38, 150);
    }, this);
    form.addRule({ui->mtu_min_download, ui->mtu_max_download}, [this] {
        return MasterDnsMtuRange(editMasterDnsInt(ui->mtu_min_download), editMasterDnsInt(ui->mtu_max_download), 200, 4000);
    }, this);
    DurationRule timeout;
    timeout.minNs = 1'000'000;
    form.bind(ui->mtu_test_timeout, Rules::Duration(timeout), this);
}
