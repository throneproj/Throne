#pragma once

#include <QList>
#include <QMap>
#include <QPair>
#include <QString>

// Plain mirror of libcore::VPNChallengeField, so the proto headers stay out of moc's way.
struct VpnAuthField {
    QString submissionKey;
    QString name;
    QString label;
    QString kind;
    QString value;
    QList<QPair<QString, QString>> options;
};

struct VpnAuthChallenge {
    QString endpointTag;
    QString id;
    QString kind;
    QString username;
    QString message;
    QString banner;
    QString error;
    QString url;
    bool echo = false;
    qint64 deadline = 0;
    QList<VpnAuthField> fields;
};

// What a person or the OTP auto-answer sends back; an empty answer acknowledges the challenge.
struct VpnAuthAnswer {
    QString username;
    QString password;
    QString secret;
    QMap<QString, QString> formValues;
};

struct VpnEndpointState {
    QString tag;
    QString state;
    QString error;
    bool connected = false;
    bool authFailed = false;
};
