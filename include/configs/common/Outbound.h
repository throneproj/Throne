#pragma once
#include <QHostInfo>
#include <utility>
#include "DialFields.h"
#include "multiplex.h"
#include "TLS.h"
#include "transport.h"
#include "xrayMultiplex.h"
#include "xrayStreamSetting.h"
#include "include/configs/baseConfig.h"

namespace Configs
{
    class outbound : public baseConfig
    {
    public:
        QString name;
        QString server;
        int server_port = 0;
        bool invalid = false;
        std::shared_ptr<DialFields> dialFields = std::make_shared<DialFields>();

        void ResolveDomainToIP(const std::function<void()> &onFinished) {
            bool noResolve = false;
            auto serverAddr = GetAddress();
            if (IsIpAddress(serverAddr) || serverAddr.isEmpty()) noResolve = true;
            if (noResolve) {
                onFinished();
                return;
            }
            QHostInfo::lookupHost(serverAddr, QApplication::instance(), [=, this](const QHostInfo &host) {
                auto addrs = host.addresses();
                if (!addrs.isEmpty()) SetAddress(addrs.first().toString());
                onFinished();
            });
        }

        virtual void SetAddress(QString newAddr) {
            server = std::move(newAddr);
        }

        virtual QString GetAddress()
        {
            return server;
        }

        virtual void SetPort(int newPort) {
            server_port = newPort;
        }

        virtual QString GetPort() {
            return QString::number(server_port);
        }

        virtual QString DisplayAddress()
        {
            return ::DisplayAddress(server, server_port);
        }

        virtual QString DisplayName()
        {
            if (name.isEmpty()) {
                return DisplayAddress();
            }
            return name;
        }

        virtual QString DisplayType() { return {}; };

        QString DisplayTypeAndName()
        {
            return QString("[%1] %2").arg(DisplayType(), DisplayName());
        }

        virtual bool IsXray() { return false; }

        virtual bool HasMux() { return false; }

        virtual bool HasTransport() { return false; }

        virtual bool HasTLS() { return false; }

        virtual bool MustTLS() { return false; }

        virtual std::shared_ptr<TLS> GetTLS() { return std::make_shared<TLS>(); }

        virtual std::shared_ptr<Transport> GetTransport() { return std::make_shared<Transport>(); }

        virtual std::shared_ptr<Multiplex> GetMux() { return std::make_shared<Multiplex>(); }

        virtual std::shared_ptr<xrayStreamSetting> GetXrayStream() { return std::make_shared<xrayStreamSetting>(); }

        virtual std::shared_ptr<xrayMultiplex> GetXrayMultiplex() { return std::make_shared<xrayMultiplex>(); }

        virtual bool IsEndpoint() { return false; };

        virtual BuildResult BuildXray() { return {}; }

        QString ExportJsonLink() {
            auto json = ExportToJson();
            QUrl url;
            url.setScheme("json");
            url.setHost("throne");
            url.setFragment(QJsonObject2QString(json, true)
                                .toUtf8()
                                .toBase64(QByteArray::Base64UrlEncoding));
            return url.toString(QUrl::FullyEncoded);
        }

        // baseConfig overrides
        bool ParseFromLink(const QString& link) override;
        bool ParseFromJson(const QJsonObject& object) override;
        QString ExportToLink() override;
        QJsonObject ExportToJson() override;
        BuildResult Build() override;
    };
}
