#include "include/ui/profile/edit_openvpn.h"

#include <QCoreApplication>
#include <QHash>
#include <QInputDialog>
#include <QJsonArray>

#include "include/database/DatabaseManager.h"
#include "include/database/OtpProfilesRepo.h"
#include "include/global/OtpPlaceholder.hpp"
#include "include/global/Utils.hpp"
#include "include/ui/utils/ValidationForm.h"
#include "include/ui/widget/json/JsonEditorDialog.h"

#include <algorithm>
#include <climits>

namespace {
    using Configs::OpenVPNAdvancedOption;
    using Kind = Configs::OpenVPNOptionKind;
    using JsonEdit::ValueType;

    const OpenVPNAdvancedOption *advancedOption(const QString &path) {
        for (const auto &option: Configs::OpenVPNAdvancedOptions()) {
            if (option.path == path) return &option;
        }
        return nullptr;
    }

    bool holdsOptions(const QString &path) {
        const auto &options = Configs::OpenVPNAdvancedOptions();
        return std::any_of(options.begin(), options.end(), [&path](const OpenVPNAdvancedOption &option) {
            return option.path.startsWith(path + '.');
        });
    }

    // Validate() names list entries like "servers[1].server".
    bool advancedField(const QString &field) {
        return advancedOption(field.left(field.indexOf('['))) != nullptr;
    }

    const QList<OpenVPNAdvancedOption> &itemFields(Kind kind) {
        static const QList<OpenVPNAdvancedOption> servers = {{"server", Kind::String}, {"server_port", Kind::Int}, {"network", Kind::String}};
        static const QList<OpenVPNAdvancedOption> pullFilters = {{"action", Kind::String}, {"text", Kind::String}};
        static const QList<OpenVPNAdvancedOption> none;
        if (kind == Kind::Servers) return servers;
        return kind == Kind::PullFilters ? pullFilters : none;
    }

    const QStringList &pageKeys() {
        static const QStringList keys = {"server", "server_port", "network", "username", "password", "static_challenge",
                                         "static_challenge_echo", "mtu", "otp_profile_id", "only_advertised_routes", "tunnel_dns",
                                         "tls.certificate", "tls.client_certificate", "tls.client_key",
                                         "tls.control_wrap.type", "tls.control_wrap.key", "tls.control_wrap.direction"};
        return keys;
    }

    JsonEdit::Issue expected(const QStringList &types, const JsonEdit::Value &value, const QString &pointer) {
        return {JsonEdit::Severity::Error,
                QObject::tr("Expected %1, got %2").arg(types.join(QObject::tr(" or ")), JsonEdit::TypeName(value.type)),
                pointer, value.span};
    }

    void checkValue(Kind kind, const JsonEdit::Value &value, const QString &pointer, QList<JsonEdit::Issue> &issues) {
        const auto string = JsonEdit::TypeName(ValueType::String), array = JsonEdit::TypeName(ValueType::Array);
        switch (kind) {
            case Kind::String:
                if (value.type != ValueType::String) issues << expected({string}, value, pointer);
                return;
            case Kind::Bool:
                if (value.type != ValueType::Bool) issues << expected({JsonEdit::TypeName(ValueType::Bool)}, value, pointer);
                return;
            case Kind::Int:
            case Kind::Int64: {
                if (value.type != ValueType::Number || !value.integral) {
                    issues << expected({QStringLiteral("integer")}, value, pointer);
                    return;
                }
                const double max = kind == Kind::Int ? INT_MAX : static_cast<double>(LLONG_MAX);
                if (value.numberValue > max) {
                    issues << JsonEdit::Issue{JsonEdit::Severity::Error, QObject::tr("Value must be at most %1").arg(max, 0, 'g', 16), pointer, value.span};
                } else if (value.numberValue < 0) {
                    // The profile keeps only positive numbers, so a negative one would vanish on save.
                    issues << JsonEdit::Issue{JsonEdit::Severity::Error, QObject::tr("Value must be at least %1").arg(0), pointer, value.span};
                }
                return;
            }
            case Kind::Lines:
                if (value.type == ValueType::String) return;
                if (value.type != ValueType::Array) {
                    issues << expected({string, array}, value, pointer);
                    return;
                }
                for (qsizetype i = 0; i < value.elements.size(); ++i) {
                    if (value.elements.at(i).type != ValueType::String) issues << expected({string}, value.elements.at(i), pointer + '/' + QString::number(i));
                }
                return;
            case Kind::Servers:
            case Kind::PullFilters:
                if (value.type != ValueType::Array) {
                    issues << expected({array}, value, pointer);
                    return;
                }
                for (qsizetype i = 0; i < value.elements.size(); ++i) {
                    const auto &item = value.elements.at(i);
                    const auto at = pointer + '/' + QString::number(i);
                    if (item.type != ValueType::Object) {
                        issues << expected({JsonEdit::TypeName(ValueType::Object)}, item, at);
                        continue;
                    }
                    for (qsizetype j = 0; j < item.keys.size(); ++j) {
                        const auto &fields = itemFields(kind);
                        const auto field = std::find_if(fields.begin(), fields.end(), [&](const OpenVPNAdvancedOption &f) { return f.path == item.keys.at(j); });
                        if (field != fields.end()) {
                            checkValue(field->kind, item.members.at(j), at + '/' + item.keys.at(j), issues);
                        } else {
                            issues << JsonEdit::Issue{JsonEdit::Severity::Error, QObject::tr("Unknown field \"%1\"").arg(item.keys.at(j)),
                                                      at + '/' + item.keys.at(j), item.keySpans.at(j)};
                        }
                    }
                }
                return;
        }
    }

    void checkObject(const JsonEdit::Value &object, const QString &prefix, const QString &pointer, QList<JsonEdit::Issue> &issues) {
        for (qsizetype i = 0; i < object.keys.size(); ++i) {
            const auto &key = object.keys.at(i);
            const auto &value = object.members.at(i);
            const auto path = prefix + key;
            const auto at = pointer + '/' + key;
            if (const auto *option = advancedOption(path)) {
                checkValue(option->kind, value, at, issues);
            } else if (holdsOptions(path)) {
                if (value.type == ValueType::Object) checkObject(value, path + '.', at, issues);
                else issues << expected({JsonEdit::TypeName(ValueType::Object)}, value, at);
            } else if (pageKeys().contains(path)) {
                issues << JsonEdit::Issue{JsonEdit::Severity::Error, EditOpenVPN::tr("Set \"%1\" on the profile page.").arg(path), at, object.keySpans.at(i)};
            } else {
                issues << JsonEdit::Issue{JsonEdit::Severity::Error, QObject::tr("Unknown field \"%1\"").arg(key), at, object.keySpans.at(i)};
            }
        }
    }

    // Unknown keys and values of the wrong type, which applying the document would drop.
    QList<JsonEdit::Issue> shapeIssues(const JsonEdit::Value &root) {
        QList<JsonEdit::Issue> issues;
        if (root.type == ValueType::Object) checkObject(root, {}, {}, issues);
        else issues << expected({JsonEdit::TypeName(ValueType::Object)}, root, {});
        return issues;
    }

    QJsonValue jsonOf(const JsonEdit::Value &value) {
        switch (value.type) {
            case ValueType::Null: return QJsonValue::Null;
            case ValueType::Bool: return value.boolValue;
            case ValueType::Number: return value.numberValue;
            case ValueType::String: return value.stringValue;
            case ValueType::Array: {
                QJsonArray array;
                for (const auto &element: value.elements) array << jsonOf(element);
                return array;
            }
            case ValueType::Object: {
                QJsonObject object;
                for (qsizetype i = 0; i < value.keys.size(); ++i) object[value.keys.at(i)] = jsonOf(value.members.at(i));
                return object;
            }
        }
        return {};
    }

    // Validate() and the checks the Advanced controls had, for the options the editor holds; `editor` adds the notes about the page.
    Validation::Issues advancedIssues(Configs::openvpn &o, bool editor) {
        using namespace Validation;
        using namespace Configs::Checks;
        Issues all;
        o.Validate(all);
        const auto add = [&all](const QString &field, const QString &message, Severity severity = Severity::Error) {
            if (!message.isEmpty()) all << Issue{severity, field, message, {}};
        };
        add("mss_fix", Int(QString::number(o.mss_fix), 0, 65535));
        add("route_metric", Int(QString::number(o.route_metric), 0, INT_MAX));
        add("renegotiate_bytes", Int(QString::number(o.renegotiate_bytes), 0, LLONG_MAX));
        add("renegotiate_packets", Int(QString::number(o.renegotiate_packets), 0, LLONG_MAX));
        add("explicit_exit_notify", Int(QString::number(o.explicit_exit_notify), 0, INT_MAX));
        add("compression", OneOf(o.compression, {"none", "no", "lz4", "lz4-v2", "stub", "stub-v2", "disabled", "off"}));
        add("compression_lzo", OneOf(o.compression_lzo, {"none", "no", "yes", "adaptive", "asym", "disabled", "off"}));
        add("allow_compression", OneOf(o.allow_compression, {"no", "asym", "yes"}));
        // The page sets the wrap's type; only a key path here is in the mode's way.
        if (!o.tls->control_wrap->key_path.isEmpty()) add("tls.control_wrap.key_path", OpenVPNModeConflict(o, "tls.control_wrap"));
        if (editor) {
            add("tls.certificate_path", VpnMaterialConflict(o.tls->certificate, o.tls->certificate_path), Severity::Warning);
            add("tls.client_certificate_path", VpnMaterialConflict(o.tls->client_certificate, o.tls->client_certificate_path), Severity::Warning);
            add("tls.client_key_path", VpnMaterialConflict(o.tls->client_key, o.tls->client_key_path), Severity::Warning);
            add("tls.control_wrap.key_path", VpnMaterialConflict(o.tls->control_wrap->key, o.tls->control_wrap->key_path), Severity::Warning);
            const auto conflicts = OpenVPNModeConflicts(o);
            if (std::any_of(conflicts.begin(), conflicts.end(), [](const QString &key) { return advancedOption(key) == nullptr; })) {
                add("mode", EditOpenVPN::tr("Static key mode does not use the credentials, certificates or control channel wrap on the profile page; clear them there."),
                    Severity::Warning);
            }
        }
        Issues issues;
        for (const auto &issue: std::as_const(all)) {
            if (advancedField(issue.field)) issues << issue;
        }
        return issues;
    }

    QString advancedErrors(Configs::openvpn &outbound) {
        QStringList lines;
        for (const auto &issue: advancedIssues(outbound, false)) {
            if (issue.severity == Validation::Severity::Error) lines << QCoreApplication::translate("Validation", "%1: %2").arg(issue.field, issue.message);
        }
        return lines.join('\n');
    }

    // Where a Validate() field sits in the document; `item` picks the list entry the issue is about.
    JsonEdit::Span issueSpan(const JsonEdit::Value &root, const QString &field, const QString &item) {
        // An issue about an option left out points at the option that needs it.
        static const QHash<QString, QString> anchors = {{"static_key", "mode"}, {"address", "mode"}, {"cipher", "mode"}, {"mss_fix", "mss_fix_mode"}};
        const JsonEdit::Value *node = &root;
        const JsonEdit::Span *key = nullptr;
        for (const auto &segment: field.split('.')) {
            const auto bracket = segment.indexOf('[');
            const int index = node->type == ValueType::Object ? node->indexOfKey(segment.left(bracket)) : -1;
            if (index < 0) {
                if (node == &root && anchors.contains(field)) return issueSpan(root, anchors.value(field), {});
                return key != nullptr ? *key : JsonEdit::Span{node->span.offset, 1};
            }
            key = &node->keySpans.at(index);
            node = &node->members.at(index);
            if (bracket >= 0) {
                const auto element = segment.mid(bracket + 1).chopped(1).toInt();
                if (node->type != ValueType::Array || element >= node->elements.size()) return *key;
                key = nullptr;
                node = &node->elements.at(element);
            }
        }
        if (node->type == ValueType::Array && !item.isEmpty()) {
            for (const auto &element: node->elements) {
                if (element.type == ValueType::String && element.stringValue == item) return element.span;
            }
        }
        if (node->type != ValueType::Array && node->type != ValueType::Object) return node->span;
        return key != nullptr ? *key : JsonEdit::Span{node->span.offset, 1};
    }

    class AdvancedValidator final : public JsonEdit::Validator {
    public:
        explicit AdvancedValidator(std::function<std::shared_ptr<Configs::openvpn>()> input) : input(std::move(input)) {}

        QList<JsonEdit::Issue> Validate(const JsonEdit::Value &root) const override {
            auto issues = shapeIssues(root);
            if (root.type != ValueType::Object) return issues;
            const auto outbound = input();
            outbound->ApplyAdvancedJson(jsonOf(root).toObject());
            for (const auto &issue: advancedIssues(*outbound, true)) {
                auto pointer = issue.field;
                pointer.replace('[', '.').remove(']').replace('.', '/');
                issues << JsonEdit::Issue{issue.severity == Validation::Severity::Error ? JsonEdit::Severity::Error : JsonEdit::Severity::Warning,
                                          issue.message, '/' + pointer, issueSpan(root, issue.field, issue.value)};
            }
            return issues;
        }

    private:
        std::function<std::shared_ptr<Configs::openvpn>()> input;
    };

    // Unknown keys and wrong types keep the editor open; the other problems block the profile's save instead.
    QStringList advancedShapeProblems(const QString &text) {
        const auto parsed = JsonEdit::Parse(text);
        if (!parsed.ok) return {};
        QStringList lines;
        for (const auto &issue: shapeIssues(parsed.root)) {
            int line = 0, column = 0;
            JsonEdit::OffsetToLineColumn(text, issue.span.offset, &line, &column);
            lines << QObject::tr("Line %1: %2").arg(line).arg(issue.message);
        }
        return lines;
    }

    // "mode, servers: [{server, server_port, network}], tls: {server_name, control_wrap: {key_path}}".
    QString keysText(const QString &prefix = {}) {
        QStringList parts, nested;
        for (const auto &option: Configs::OpenVPNAdvancedOptions()) {
            if (!option.path.startsWith(prefix)) continue;
            const auto rest = option.path.mid(prefix.size());
            if (const auto dot = rest.indexOf('.'); dot >= 0) {
                const auto head = rest.left(dot);
                if (nested.contains(head)) continue;
                nested << head;
                parts << QStringLiteral("%1: {%2}").arg(head, keysText(prefix + head + '.'));
            } else if (const auto &fields = itemFields(option.kind); !fields.isEmpty()) {
                QStringList keys;
                for (const auto &field: fields) keys << field.path;
                parts << QStringLiteral("%1: [{%2}]").arg(rest, keys.join(QStringLiteral(", ")));
            } else {
                parts << rest;
            }
        }
        return parts.join(QStringLiteral(", "));
    }
}

EditOpenVPN::EditOpenVPN(QWidget *parent) : QWidget(parent), ui(new Ui::EditOpenVPN) {
    ui->setupUi(this);

    ui->network->addItems(Configs::openvpnNetworks);
    ui->control_wrap_type->addItems({"", "tls_auth", "tls_crypt", "tls_crypt_v2"});
    ui->control_wrap_direction->addItems({"", "server", "client"});
    ui->tunnel_dns->addItem(tr("None"), QString(Configs::kTunnelDnsNone));
    ui->tunnel_dns->addItem(tr("Prefer"), QString(Configs::kTunnelDnsPrefer));
    ui->tunnel_dns->addItem(tr("Strict"), QString(Configs::kTunnelDnsStrict));

    ui->otp_profile_l->setToolTip(tr("OTP entry whose code replaces %1 and answers static challenges.")
                                      .arg(Configs::kOtpPlaceholder));
    ui->password->setPlaceholderText(tr("%1 is replaced at connect time").arg(Configs::kOtpPlaceholder));

    connect(ui->certificate, &QPushButton::clicked, this,
            [=, this] { editPem(ui->certificate, tr("CA Certificate"), CACHE.certificate); });
    connect(ui->client_certificate, &QPushButton::clicked, this,
            [=, this] { editPem(ui->client_certificate, tr("Client Certificate"), CACHE.clientCertificate); });
    connect(ui->client_key, &QPushButton::clicked, this,
            [=, this] { editPem(ui->client_key, tr("Client Key"), CACHE.clientKey); });
    connect(ui->control_wrap_key, &QPushButton::clicked, this,
            [=, this] { editPem(ui->control_wrap_key, tr("Control Channel Wrap Key"), CACHE.controlWrapKey); });
    connect(ui->advanced_button, &QPushButton::clicked, this, &EditOpenVPN::editAdvanced);
}

EditOpenVPN::~EditOpenVPN() {
    delete ui;
}

void EditOpenVPN::editPem(QPushButton *button, const QString &title, QStringList &target) {
    bool ok;
    auto txt = QInputDialog::getMultiLineText(this, title, "", target.join("\n"), &ok);
    if (!ok) return;
    target = txt.split("\n", Qt::SkipEmptyParts);
    if (editor_cache_updated) editor_cache_updated();
    else button->setText(target.isEmpty() ? tr("Not set") : tr("Already set"));
    revalidateInput();
}

void EditOpenVPN::editAdvanced() {
    if (ent == nullptr) return;
    auto *outbound = ent->OpenVPN();
    auto *editor = new JsonEdit::JsonEditorDialog(outbound->ExportAdvancedJson(), this);
    editor->setWindowTitle(tr("OpenVPN Advanced"));
    editor->SetValidator(std::make_shared<AdvancedValidator>([this] { return withInput(); }));
    editor->SetPlaceholderText(tr("No advanced options are set. Available keys:") + '\n' + keysText());
    editor->SetAcceptCheck(advancedShapeProblems);
    const auto advanced = editor->OpenEditor();
    const bool accepted = editor->result() == QDialog::Accepted;
    editor->deleteLater();
    if (!accepted) return;

    outbound->ApplyAdvancedJson(advanced);
    if (editor_cache_updated) editor_cache_updated();
    else ui->advanced_button->setText(outbound->ExportAdvancedJson().isEmpty() ? tr("Not set") : tr("Already set"));
    revalidateInput();
    // The server list decides whether the outer address and port are needed.
    if (editor_state_changed) editor_state_changed();
}

QList<QPair<QPushButton *, QString>> EditOpenVPN::get_editor_cached() {
    const auto advanced = ent == nullptr ? QJsonObject() : ent->OpenVPN()->ExportAdvancedJson();
    return {
        {ui->certificate, CACHE.certificate.join("\n")},
        {ui->client_certificate, CACHE.clientCertificate.join("\n")},
        {ui->client_key, CACHE.clientKey.join("\n")},
        {ui->control_wrap_key, CACHE.controlWrapKey.join("\n")},
        {ui->advanced_button, advanced.isEmpty() ? QString() : QJsonObject2QString(advanced, true)},
    };
}

void EditOpenVPN::onStart(std::shared_ptr<Configs::Profile> _ent) {
    this->ent = _ent;
    auto outbound = this->ent->OpenVPN();

    ui->network->setCurrentText(outbound->network);
    ui->username->setText(outbound->username);
    ui->password->setText(outbound->password);
    ui->static_challenge->setText(outbound->static_challenge);
    ui->static_challenge_echo->setChecked(outbound->static_challenge_echo);
    ui->mtu->setText(outbound->mtu == 0 ? QString() : Int2String(outbound->mtu));
    ui->only_advertised_routes->setChecked(outbound->only_advertised_routes);
    auto tunnelDnsIndex = ui->tunnel_dns->findData(outbound->tunnel_dns);
    if (tunnelDnsIndex < 0) tunnelDnsIndex = ui->tunnel_dns->findData(QString(Configs::kTunnelDnsPrefer));
    ui->tunnel_dns->setCurrentIndex(tunnelDnsIndex);

    ui->otp_profile->addItem(tr("None"), -1);
    for (const auto &otp: Configs::dataManager->otpProfilesRepo->GetAllOtpProfiles()) {
        ui->otp_profile->addItem(otp->DisplayName(), otp->id);
    }
    auto otpIndex = ui->otp_profile->findData(outbound->otp_profile_id);
    ui->otp_profile->setCurrentIndex(otpIndex < 0 ? 0 : otpIndex);

    ui->control_wrap_type->setCurrentText(outbound->tls->control_wrap->type);
    ui->control_wrap_direction->setCurrentText(outbound->tls->control_wrap->direction);

    CACHE.certificate = outbound->tls->certificate;
    CACHE.clientCertificate = outbound->tls->client_certificate;
    CACHE.clientKey = outbound->tls->client_key;
    CACHE.controlWrapKey = outbound->tls->control_wrap->key;
}

bool EditOpenVPN::onEnd() {
    saveInto(*ent->OpenVPN());
    return true;
}

void EditOpenVPN::saveInto(Configs::openvpn &outbound) const {
    outbound.network = ui->network->currentText().trimmed();
    outbound.username = ui->username->text().trimmed();
    outbound.password = ui->password->text();
    outbound.static_challenge = ui->static_challenge->text();
    outbound.static_challenge_echo = ui->static_challenge_echo->isChecked();
    outbound.mtu = ui->mtu->text().trimmed().toInt();
    outbound.otp_profile_id = ui->otp_profile->currentData().toInt();
    outbound.only_advertised_routes = ui->only_advertised_routes->isChecked();
    outbound.tunnel_dns = ui->tunnel_dns->currentData().toString();

    outbound.tls->certificate = CACHE.certificate;
    outbound.tls->client_certificate = CACHE.clientCertificate;
    outbound.tls->client_key = CACHE.clientKey;
    outbound.tls->control_wrap->type = ui->control_wrap_type->currentText().trimmed();
    outbound.tls->control_wrap->key = CACHE.controlWrapKey;
    outbound.tls->control_wrap->direction = ui->control_wrap_direction->currentText().trimmed();
}

std::shared_ptr<Configs::openvpn> EditOpenVPN::withInput() const {
    auto outbound = ent->OpenVPN()->DetachedCopy();
    saveInto(*outbound);
    return outbound;
}

void EditOpenVPN::revalidateInput() {
    if (!form) return;
    for (auto *widget: std::as_const(checkedWidgets)) form->revalidate(widget);
}

void EditOpenVPN::bindValidation(Validation::Form &form) {
    using namespace Validation;
    using namespace Configs::Checks;
    using Check = std::function<QString(Configs::openvpn &)>;
    this->form = &form;
    form.bind(ui->mtu, Rules::Int(0, 65535), this);

    // The Advanced editor's options, such as the mode, the file paths and the fingerprints, are already applied to the profile.
    const auto rule = [this, &form](const QList<QWidget *> &participants, const Check &check, const QString &label = {}) {
        form.addRule(participants, [this, check] { return check(*withInput()); }, this, Severity::Error, label);
        for (auto *widget: participants) {
            if (!checkedWidgets.contains(widget)) checkedWidgets << widget;
        }
    };
    // The wrap rows are labelled just Type, Key and Direction inside their group.
    const auto wrapType = tr("Control Channel Wrap Type"), wrapKey = tr("Control Channel Wrap Key");
    rule({ui->certificate}, OpenVPNTlsVerification);
    rule({ui->client_certificate, ui->client_key}, OpenVPNClientPair);
    rule({ui->certificate}, [](const Configs::openvpn &o) { return VpnMaterialConflict(o.tls->certificate, o.tls->certificate_path); });
    rule({ui->client_certificate}, [](const Configs::openvpn &o) {
        return VpnMaterialConflict(o.tls->client_certificate, o.tls->client_certificate_path);
    });
    rule({ui->client_key}, [](const Configs::openvpn &o) { return VpnMaterialConflict(o.tls->client_key, o.tls->client_key_path); });
    rule({ui->control_wrap_key}, [](const Configs::openvpn &o) {
        return VpnMaterialConflict(o.tls->control_wrap->key, o.tls->control_wrap->key_path);
    }, wrapKey);
    rule({ui->control_wrap_type, ui->control_wrap_key}, OpenVPNControlWrapKey, wrapType);
    rule({ui->control_wrap_direction, ui->control_wrap_type}, OpenVPNControlWrapDirection, tr("Control Channel Wrap Direction"));

    const QList<QPair<QWidget *, QString>> modeBound = {
        {ui->username, "username"},
        {ui->password, "password"},
        {ui->static_challenge, "static_challenge"},
        {ui->static_challenge_echo, "static_challenge_echo"},
        {ui->certificate, "tls.certificate"},
        {ui->client_certificate, "tls.client_certificate"},
        {ui->client_key, "tls.client_key"},
    };
    for (const auto &bound: modeBound) {
        rule({bound.first}, [key = bound.second](const Configs::openvpn &o) { return OpenVPNModeConflict(o, key); });
    }
    rule({ui->control_wrap_type}, [](const Configs::openvpn &o) { return OpenVPNModeConflict(o, "tls.control_wrap"); }, wrapType);

    rule({ui->username, ui->otp_profile}, [](const Configs::openvpn &o) { return OtpPlaceholderUnbound(o.username, o.otp_profile_id); });
    rule({ui->password, ui->otp_profile}, [](const Configs::openvpn &o) { return OtpPlaceholderUnbound(o.password, o.otp_profile_id); });
    rule({ui->advanced_button}, advancedErrors);
}

bool EditOpenVPN::serverPortOptional() {
    return ent != nullptr && !ent->OpenVPN()->servers.isEmpty();
}

bool EditOpenVPN::serverAddressOptional() {
    return serverPortOptional();
}
