#pragma once
#include <QCoreApplication>
#include "include/configs/common/Outbound.h"

namespace Configs
{
    namespace Checks {
        // google/shlex Split, as ThroneCore reads the arguments: quotes must close and a backslash needs a character after it.
        inline QString ExtraCoreArgs(const QString &text)
        {
            enum { Start, Word, Escape, Single, Double, DoubleEscape, Comment } state = Start;
            for (const QChar c : text) {
                switch (state) {
                    case Start:
                        if (c == '#') state = Comment;
                        else if (c.isSpace()) break;
                        else if (c == '\'') state = Single;
                        else if (c == '"') state = Double;
                        else if (c == '\\') state = Escape;
                        else state = Word;
                        break;
                    case Word:
                        if (c.isSpace()) state = Start;
                        else if (c == '\'') state = Single;
                        else if (c == '"') state = Double;
                        else if (c == '\\') state = Escape;
                        break;
                    case Escape: state = Word; break;
                    case Single:
                        if (c == '\'') state = Word;
                        break;
                    case Double:
                        if (c == '"') state = Word;
                        else if (c == '\\') state = DoubleEscape;
                        break;
                    case DoubleEscape: state = Double; break;
                    case Comment:
                        if (c == '\n') state = Start;
                        break;
                }
            }
            if (state == Single || state == Double || state == DoubleEscape) {
                return QCoreApplication::translate("Validation", "A quote is not closed; end it with the same quote mark.");
            }
            if (state == Escape) return QCoreApplication::translate("Validation", "The arguments end with a backslash; remove it or add the character it escapes.");
            return {};
        }

        // ThroneCore writes the config to a file and puts its path into the first argument containing %s.
        inline QString ExtraCoreConfigArg(const QString &args, const QString &config)
        {
            if (config.trimmed().isEmpty() || args.contains("%s")) return {};
            return QCoreApplication::translate("Validation", "Add %s to the arguments where the config file path goes; without it the config is not used.");
        }
    }

    class extracore : public outbound {
    public:
        bool HasServerEndpoint() override { return false; }

        QString socksAddress = "127.0.0.1";
        int socksPort = 1080;
        QString extraCorePath;
        QString extraCoreArgs;
        QString extraCoreConf;
        bool noLogs = false;

        bool ParseFromJson(const QJsonObject &object) override {
            if (object.isEmpty()) return false;
            if (object.contains("name")) name = object["name"].toString();
            if (object.contains("socks_address")) socksAddress = object["socks_address"].toString();
            if (object.contains("socks_port")) socksPort = object["socks_port"].toInt();
            if (object.contains("extra_core_path")) extraCorePath = object["extra_core_path"].toString();
            if (object.contains("extra_core_args")) extraCoreArgs = object["extra_core_args"].toString();
            if (object.contains("extra_core_conf")) extraCoreConf = object["extra_core_conf"].toString();
            if (object.contains("no_logs")) noLogs = object["no_logs"].toBool();
            return true;
        }

        QJsonObject ExportToJson() override {
            QJsonObject object;
            object["name"] = name;
            object["type"] = "extracore";
            object["socks_address"] = socksAddress;
            object["socks_port"] = socksPort;
            object["extra_core_path"] = extraCorePath;
            object["extra_core_args"] = extraCoreArgs;
            object["extra_core_conf"] = extraCoreConf;
            object["no_logs"] = noLogs;
            return object;
        }

        QString DisplayType() override { return "ExtraCore"; };

        // The config is owned by an external core, so we cannot inspect it.
        SecurityInfo GetSecurity() override { return {}; }

        bool IsExtraCore() override { return true; }

        void Validate(Validation::Issues &issues) override
        {
            outbound::Validate(issues);
            const auto report = [&issues](const QString &field, const QString &message, const QString &value = {}) {
                if (!message.isEmpty()) issues << Validation::Issue{Validation::Severity::Error, field, message, value};
            };
            report("socks_address", Validation::Host(socksAddress), socksAddress);
            report("socks_port", Validation::Port(QString::number(socksPort)), QString::number(socksPort));
            report("extra_core_path", Validation::Required(extraCorePath));
            report("extra_core_args", Checks::ExtraCoreArgs(extraCoreArgs));
            report("extra_core_conf", Checks::ExtraCoreConfigArg(extraCoreArgs, extraCoreConf));
        }

        BuildResult Build() override
        {
            QJsonObject socksOutbound;
            socksOutbound["type"] = "socks";
            socksOutbound["server"] = socksAddress;
            socksOutbound["server_port"] = socksPort;
            return {socksOutbound, ""};
        }
    };
}
