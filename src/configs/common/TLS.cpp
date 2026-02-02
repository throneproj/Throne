#include "include/configs/common/TLS.h"

#include <QJsonArray>
#include <include/global/Utils.hpp>

#include "include/configs/common/utils.h"



namespace Configs {
    bool uTLS::ParseFromLink(const QString& link)
    {
        auto url = QUrl(link);
        if (!url.isValid() && !url.errorString().startsWith("Invalid port")) return false;
        auto query = QUrlQuery(url.query(QUrl::ComponentFormattingOption::FullyDecoded));

        // handle the common format
        if (query.hasQueryItem("fp")) fingerPrint = query.queryItemValue("fp");
        if (!fingerPrint.isEmpty()) enabled = true;
        return true;
    }
    bool uTLS::ParseFromJson(const QJsonObject& object)
    {
        if (object.isEmpty()) return false;
        if (object.contains("enabled")) enabled = object["enabled"].toBool();
        if (object.contains("fingerprint")) fingerPrint = object["fingerprint"].toString();
        return true;
    }
    QString uTLS::ExportToLink()
    {
        QUrlQuery query;
        if (!enabled) return "";
        if (!fingerPrint.isEmpty()) query.addQueryItem("fp", fingerPrint);
        return query.toString();
    }
    QJsonObject uTLS::ExportToJson()
    {
        QJsonObject object;
        if (!enabled) return object;
        object["enabled"] = enabled;
        if (!fingerPrint.isEmpty()) object["fingerprint"] = fingerPrint;
        return object;
    }
    BuildResult uTLS::Build()
    {
        if (!supported) return {};
        auto obj = ExportToJson();
        if ((obj.isEmpty() || obj["enabled"].toBool() == false || fingerPrint.isEmpty()) && !Configs::dataManager->settingsRepo->utlsFingerprint.isEmpty()) {
            obj["enabled"] = true;
            obj["fingerprint"] = Configs::dataManager->settingsRepo->utlsFingerprint;
        }
        return {obj, ""};
    }

    bool ECH::ParseFromLink(const QString& link)
    {
        auto url = QUrl(link);
        if (!url.isValid() && !url.errorString().startsWith("Invalid port")) return false;
        auto query = QUrlQuery(url.query(QUrl::ComponentFormattingOption::FullyDecoded));

        if (query.hasQueryItem("ech_enabled")) enabled = query.queryItemValue("ech_enabled") == "true";
        if (query.hasQueryItem("ech_config")) config = query.queryItemValue("ech_config").split(",");
        if (query.hasQueryItem("ech_config_path")) config_path = query.queryItemValue("ech_config_path");
        return true;
    }
    bool ECH::ParseFromJson(const QJsonObject& object)
    {
        if (object.isEmpty()) return false;
        if (object.contains("enabled")) enabled = object["enabled"].toBool();
        if (object.contains("config")) {
            config = QJsonArray2QListString(object["config"].toArray());
        }
        if (object.contains("config_path")) config_path = object["config_path"].toString();
        return true;
    }
    QString ECH::ExportToLink()
    {
        QUrlQuery query;
        if (!enabled) return "";
        query.addQueryItem("ech_enabled", "true");
        if (!config.isEmpty()) query.addQueryItem("ech_config", config.join(","));
        if (!config_path.isEmpty()) query.addQueryItem("ech_config_path", config_path);
        return query.toString();
    }
    QJsonObject ECH::ExportToJson()
    {
        QJsonObject object;
        if (!enabled) return object;
        object["enabled"] = enabled;
        if (!config.isEmpty()) {
            object["config"] = QListStr2QJsonArray(config);
        }
        if (!config_path.isEmpty()) object["config_path"] = config_path;
        return object;
    }
    BuildResult ECH::Build()
    {
        return {ExportToJson(), ""};
    }

    bool Reality::ParseFromLink(const QString& link)
    {
        auto url = QUrl(link);
        if (!url.isValid() && !url.errorString().startsWith("Invalid port")) return false;
        auto query = QUrlQuery(url.query(QUrl::ComponentFormattingOption::FullyDecoded));

        // handle the common format
        if (query.hasQueryItem("pbk"))
        {
            enabled = true;
            public_key = query.queryItemValue("pbk");
            short_id = query.queryItemValue("sid");
        }
        return true;
    }
    bool Reality::ParseFromJson(const QJsonObject& object)
    {
        if (object.isEmpty()) return false;
        if (object.contains("enabled")) enabled = object["enabled"].toBool();
        if (object.contains("public_key")) public_key = object["public_key"].toString();
        if (object.contains("short_id")) short_id = object["short_id"].toString();
        return true;
    }
    QString Reality::ExportToLink()
    {
        QUrlQuery query;
        if (!enabled) return "";
        query.addQueryItem("pbk", public_key);
        query.addQueryItem("sid", short_id);
        return query.toString();
    }
    QJsonObject Reality::ExportToJson()
    {
        QJsonObject object;
        if (!enabled) return object;
        object["enabled"] = enabled;
        if (!public_key.isEmpty()) object["public_key"] = public_key;
        if (!short_id.isEmpty()) object["short_id"] = short_id;
        return object;
    }
    BuildResult Reality::Build()
    {
        QJsonObject object;
        if (!public_key.isEmpty() || enabled) {
            object["enabled"] = true;
            object["public_key"] = public_key;
            if (!short_id.isEmpty()) object["short_id"] = short_id;
        }
        return {object, ""};
    }

    bool TLS::ParseFromLink(const QString& link)
    {
        auto url = QUrl(link);
        if (!url.isValid() && !url.errorString().startsWith("Invalid port")) return false;
        auto query = QUrlQuery(url.query(QUrl::ComponentFormattingOption::FullyDecoded));

        if (query.hasQueryItem("security")) enabled = query.queryItemValue("security")
        .replace("reality", "tls")
        .replace("none", "")
        .replace("0", "")
        .replace("false", "tls")
        .replace("1", "tls")
        .replace("true", "tls") == "tls";
        if (query.hasQueryItem("disable_sni")) disable_sni = query.queryItemValue("disable_sni").replace("1", "true") == "true";
        if (query.hasQueryItem("sni")) server_name = query.queryItemValue("sni");
        if (query.hasQueryItem("peer")) server_name = query.queryItemValue("peer");
        if (query.hasQueryItem("allowInsecure")) insecure = query.queryItemValue("allowInsecure").replace("1", "true") == "true";
        if (query.hasQueryItem("allow_insecure")) insecure = query.queryItemValue("allow_insecure").replace("1", "true") == "true";
        if (query.hasQueryItem("insecure")) insecure = query.queryItemValue("insecure").replace("1", "true") == "true";
        if (query.hasQueryItem("alpn")) alpn = query.queryItemValue("alpn").split(",");
        if (query.hasQueryItem("tls_min_version")) min_version = query.queryItemValue("tls_min_version");
        if (query.hasQueryItem("tls_max_version")) max_version = query.queryItemValue("tls_max_version");
        if (query.hasQueryItem("tls_cipher_suites")) cipher_suites = query.queryItemValue("tls_cipher_suites").split(",");
        if (query.hasQueryItem("tls_curve_preferences")) curve_preferences = query.queryItemValue("tls_curve_preferences").split(",");
        if (query.hasQueryItem("tls_certificate")) certificate = query.queryItemValue("tls_certificate").split(",");
        if (query.hasQueryItem("tls_certificate_path")) certificate_path = query.queryItemValue("tls_certificate_path");
        if (query.hasQueryItem("tls_certificate_public_key_sha256")) certificate_public_key_sha256 = query.queryItemValue("tls_certificate_public_key_sha256").split(",");
        if (query.hasQueryItem("tls_client_certificate")) client_certificate = query.queryItemValue("tls_client_certificate").split(",");
        if (query.hasQueryItem("tls_client_certificate_path")) client_certificate_path = query.queryItemValue("tls_client_certificate_path");
        if (query.hasQueryItem("tls_client_key")) client_key = query.queryItemValue("tls_client_key").split(",");
        if (query.hasQueryItem("tls_client_key_path")) client_key_path = query.queryItemValue("tls_client_key_path");
        if (query.hasQueryItem("tls_fragment")) fragment = query.queryItemValue("tls_fragment") == "true";
        if (query.hasQueryItem("tls_fragment_fallback_delay")) fragment_fallback_delay = query.queryItemValue("tls_fragment_fallback_delay");
        if (query.hasQueryItem("tls_record_fragment")) record_fragment = query.queryItemValue("tls_record_fragment") == "true";
        if (!server_name.isEmpty()) enabled = true;
        ech->ParseFromLink(link);
        utls->ParseFromLink(link);
        reality->ParseFromLink(link);
        return true;
    }
    bool TLS::ParseFromJson(const QJsonObject& object)
    {
        if (object.isEmpty()) return false;
        if (object.contains("enabled")) enabled = object["enabled"].toBool();
        if (object.contains("disable_sni")) disable_sni = object["disable_sni"].toBool();
        if (object.contains("server_name")) server_name = object["server_name"].toString();
        if (object.contains("insecure")) insecure = object["insecure"].toBool();
        if (object.contains("alpn")) {
            alpn = QJsonArray2QListString(object["alpn"].toArray());
        }
        if (object.contains("min_version")) min_version = object["min_version"].toString();
        if (object.contains("max_version")) max_version = object["max_version"].toString();
        if (object.contains("cipher_suites")) {
            cipher_suites = QJsonArray2QListString(object["cipher_suites"].toArray());
        }
        if (object.contains("curve_preferences")) {
            curve_preferences = QJsonArray2QListString(object["curve_preferences"].toArray());
        }
        if (object.contains("certificate")) {
            if (object["certificate"].isString()) {
                certificate = object["certificate"].toString().split("\n", Qt::SkipEmptyParts);
            } else {
                certificate = QJsonArray2QListString(object["certificate"].toArray());
            }
        }
        if (object.contains("certificate_path")) certificate_path = object["certificate_path"].toString();
        if (object.contains("certificate_public_key_sha256")) {
            certificate_public_key_sha256 = QJsonArray2QListString(object["certificate_public_key_sha256"].toArray());
        }
        if (object.contains("client_certificate")) {
            client_certificate = QJsonArray2QListString(object["client_certificate"].toArray());
        }
        if (object.contains("client_certificate_path")) client_certificate_path = object["client_certificate_path"].toString();
        if (object.contains("client_key")) {
            client_key = QJsonArray2QListString(object["client_key"].toArray());
        }
        if (object.contains("client_key_path")) client_key_path = object["client_key_path"].toString();
        if (object.contains("fragment")) fragment = object["fragment"].toBool();
        if (object.contains("fragment_fallback_delay")) fragment_fallback_delay = object["fragment_fallback_delay"].toString();
        if (object.contains("record_fragment")) record_fragment = object["record_fragment"].toBool();
        if (object.contains("ech")) ech->ParseFromJson(object["ech"].toObject());
        if (object.contains("utls")) utls->ParseFromJson(object["utls"].toObject());
        if (object.contains("reality")) reality->ParseFromJson(object["reality"].toObject());
        return true;
    }
    QString TLS::ExportToLink()
    {
        QUrlQuery query;
        if (!enabled) return "";
        query.addQueryItem("security", reality->enabled ? "reality" : "tls");
        if (disable_sni) query.addQueryItem("disable_sni", "true");
        if (!server_name.isEmpty()) query.addQueryItem("sni", server_name);
        if (insecure) query.addQueryItem("allowInsecure", "true");
        if (!alpn.isEmpty()) query.addQueryItem("alpn", alpn.join(","));
        if (!min_version.isEmpty()) query.addQueryItem("tls_min_version", min_version);
        if (!max_version.isEmpty()) query.addQueryItem("tls_max_version", max_version);
        if (!cipher_suites.isEmpty()) query.addQueryItem("tls_cipher_suites", cipher_suites.join(","));
        if (!curve_preferences.isEmpty()) query.addQueryItem("tls_curve_preferences", curve_preferences.join(","));
        if (!certificate.isEmpty()) query.addQueryItem("tls_certificate", certificate.join(","));
        if (!certificate_path.isEmpty()) query.addQueryItem("tls_certificate_path", certificate_path);
        if (!certificate_public_key_sha256.isEmpty()) query.addQueryItem("tls_certificate_public_key_sha256", certificate_public_key_sha256.join(","));
        if (!client_certificate.isEmpty()) query.addQueryItem("tls_client_certificate", client_certificate.join(","));
        if (!client_certificate_path.isEmpty()) query.addQueryItem("tls_client_certificate_path", client_certificate_path);
        if (!client_key.isEmpty()) query.addQueryItem("tls_client_key", client_key.join(","));
        if (!client_key_path.isEmpty()) query.addQueryItem("tls_client_key_path", client_key_path);
        if (fragment) query.addQueryItem("tls_fragment", "true");
        if (!fragment_fallback_delay.isEmpty()) query.addQueryItem("tls_fragment_fallback_delay", fragment_fallback_delay);
        if (record_fragment) query.addQueryItem("tls_record_fragment", "true");
        mergeUrlQuery(query, ech->ExportToLink());
        mergeUrlQuery(query, utls->ExportToLink());
        mergeUrlQuery(query, reality->ExportToLink());
        return query.toString();
    }
    QJsonObject TLS::ExportToJson()
    {
        QJsonObject object;
        if (!enabled) return object;
        object["enabled"] = enabled;
        if (disable_sni) object["disable_sni"] = disable_sni;
        if (!server_name.isEmpty()) object["server_name"] = server_name;
        if (insecure) object["insecure"] = insecure;
        if (!alpn.isEmpty()) {
            object["alpn"] = QListStr2QJsonArray(alpn);
        }
        if (!min_version.isEmpty()) object["min_version"] = min_version;
        if (!max_version.isEmpty()) object["max_version"] = max_version;
        if (!cipher_suites.isEmpty()) {
            object["cipher_suites"] = QListStr2QJsonArray(cipher_suites);
        }
        if (!curve_preferences.isEmpty()) {
            object["curve_preferences"] = QListStr2QJsonArray(curve_preferences);
        }
        if (!certificate.isEmpty()) {
            object["certificate"] = QListStr2QJsonArray(certificate);
        }
        if (!certificate_path.isEmpty()) object["certificate_path"] = certificate_path;
        if (!certificate_public_key_sha256.isEmpty()) {
            object["certificate_public_key_sha256"] = QListStr2QJsonArray(certificate_public_key_sha256);
        }
        if (!client_certificate.isEmpty()) object["client_certificate"] = QListStr2QJsonArray(client_certificate);
        if (!client_certificate_path.isEmpty()) object["client_certificate_path"] = client_certificate_path;
        if (!client_key.isEmpty()) {
            object["client_key"] = QListStr2QJsonArray(client_key);
        }
        if (!client_key_path.isEmpty()) object["client_key_path"] = client_key_path;
        if (fragment) object["fragment"] = fragment;
        if (!fragment_fallback_delay.isEmpty()) object["fragment_fallback_delay"] = fragment_fallback_delay;
        if (record_fragment) object["record_fragment"] = record_fragment;
        if (ech->enabled) object["ech"] = ech->ExportToJson();
        if (utls->enabled) object["utls"] = utls->ExportToJson();
        if (reality->enabled) object["reality"] = reality->ExportToJson();
        return object;
    }
    BuildResult TLS::Build()
    {
        QJsonObject object;
        if (!enabled) return {};
        object["enabled"] = enabled;
        if (disable_sni) object["disable_sni"] = disable_sni;
        if (!server_name.isEmpty()) object["server_name"] = server_name;
        if (insecure) object["insecure"] = insecure;
        if (!alpn.isEmpty()) {
            object["alpn"] = QListStr2QJsonArray(alpn);
        }
        if (!min_version.isEmpty()) object["min_version"] = min_version;
        if (!max_version.isEmpty()) object["max_version"] = max_version;
        if (!cipher_suites.isEmpty()) {
            object["cipher_suites"] = QListStr2QJsonArray(cipher_suites);
        }
        if (!curve_preferences.isEmpty()) {
            object["curve_preferences"] = QListStr2QJsonArray(curve_preferences);
        }
        if (!certificate.isEmpty()) {
            object["certificate"] = QListStr2QJsonArray(certificate);
        }
        if (!certificate_path.isEmpty()) object["certificate_path"] = certificate_path;
        if (!certificate_public_key_sha256.isEmpty()) {
            object["certificate_public_key_sha256"] = QListStr2QJsonArray(certificate_public_key_sha256);
        }
        if (!client_certificate.isEmpty()) object["client_certificate"] = QListStr2QJsonArray(client_certificate);
        if (!client_certificate_path.isEmpty()) object["client_certificate_path"] = client_certificate_path;
        if (!client_key.isEmpty()) {
            object["client_key"] = QListStr2QJsonArray(client_key);
        }
        if (!client_key_path.isEmpty()) object["client_key_path"] = client_key_path;
        if (fragment) object["fragment"] = fragment;
        if (!fragment_fallback_delay.isEmpty()) object["fragment_fallback_delay"] = fragment_fallback_delay;
        if (record_fragment) object["record_fragment"] = record_fragment;
        if (auto obj = ech->Build().object;!obj.isEmpty()) object["ech"] = obj;
        if (auto obj = utls->Build().object;!obj.isEmpty()) {
            object["utls"] = obj;
        } else if (reality->enabled) {
            object["utls"] = QJsonObject{
                    {"enabled", true},
                    {"fingerprint", "random"},
                };
        }
        if (auto obj = reality->Build().object;!obj.isEmpty()) object["reality"] = obj;
        return {object, ""};
    }
}


