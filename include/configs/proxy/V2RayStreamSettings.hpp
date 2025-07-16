#pragma once

#include "AbstractBean.hpp"
#include "Preset.hpp"

namespace Configs {
    class V2rayStreamSettings : public JsonStore {
    public:
        QString network = "tcp";
        QString security = "";
        QString packet_encoding = "";

        QString path = "";
        QString host = "";
        QString method = "";
        QString headers = "";
        QString header_type = "";

        QString sni = "";
        QString alpn = "";
        QString certificate = "";
        QString utlsFingerprint = "";
        bool allow_insecure = false;
        // ws early data
        QString ws_early_data_name = "";
        int ws_early_data_length = 0;
        // reality
        QString reality_pbk = "";
        QString reality_sid = "";

        V2rayStreamSettings() : JsonStore() {
            _add(new configItem("net", &network, itemType::string));
            _add(new configItem("sec", &security, itemType::string));
            _add(new configItem("pac_enc", &packet_encoding, itemType::string));
            _add(new configItem("path", &path, itemType::string));
            _add(new configItem("host", &host, itemType::string));
            _add(new configItem("sni", &sni, itemType::string));
            _add(new configItem("alpn", &alpn, itemType::string));
            _add(new configItem("cert", &certificate, itemType::string));
            _add(new configItem("insecure", &allow_insecure, itemType::boolean));
            _add(new configItem("headers", &headers, itemType::string));
            _add(new configItem("h_type", &header_type, itemType::string));
            _add(new configItem("method", &method, itemType::string));
            _add(new configItem("ed_name", &ws_early_data_name, itemType::string));
            _add(new configItem("ed_len", &ws_early_data_length, itemType::integer));
            _add(new configItem("utls", &utlsFingerprint, itemType::string));
            _add(new configItem("pbk", &reality_pbk, itemType::string));
            _add(new configItem("sid", &reality_sid, itemType::string));
        }

        void BuildStreamSettingsSingBox(QJsonObject *outbound);

        QMap<QString, QString> GetHeaderPairs(bool* ok) {
            bool inQuote = false;
            QString curr;
            QStringList list;
            for (const auto &ch: headers) {
                if (inQuote) {
                    if (ch == '"') {
                        inQuote = false;
                        list << curr;
                        curr = "";
                        continue;
                    } else {
                        curr += ch;
                        continue;
                    }
                }
                if (ch == '"') {
                    inQuote = true;
                    continue;
                }
                if (ch == ' ') {
                    if (!curr.isEmpty()) {
                        list << curr;
                        curr = "";
                    }
                    continue;
                }
                if (ch == '=') {
                    if (!curr.isEmpty()) {
                        list << curr;
                        curr = "";
                    }
                    continue;
                }
                curr+=ch;
            }
            if (!curr.isEmpty()) list<<curr;

            if (list.size()%2 == 1) {
                *ok = false;
                return {};
            }
            QMap<QString,QString> res;
            for (int i = 0; i < list.size(); i+=2) {
                res[list[i]] = list[i + 1];
            }
            *ok = true;
            return res;
        }
    };

    inline V2rayStreamSettings *GetStreamSettings(AbstractBean *bean) {
        if (bean == nullptr) return nullptr;
        auto stream_item = bean->_get("stream");
        if (stream_item != nullptr) {
            auto stream_store = (JsonStore *) stream_item->ptr;
            auto stream = (Configs::V2rayStreamSettings *) stream_store;
            return stream;
        }
        return nullptr;
    }
} // namespace Configs
