#pragma once

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>

namespace OTP {
    enum class Algorithm : int {
        SHA1 = 0,
        SHA256,
        SHA512,
    };

    enum class Type : int {
        TOTP = 0,
        HOTP,
    };

    constexpr int DEFAULT_DIGITS = 6;
    constexpr int DEFAULT_PERIOD = 30;
    constexpr int MIN_DIGITS = 4;
    constexpr int MAX_DIGITS = 10;
    constexpr int MIN_PERIOD = 1;
    constexpr int MAX_PERIOD = 3600;

    struct Entry {
        QString name;
        QString issuer;
        QString secret;
        Algorithm algorithm = Algorithm::SHA1;
        Type type = Type::TOTP;
        int digits = DEFAULT_DIGITS;
        int period = DEFAULT_PERIOD;
        qint64 counter = 0;
    };

    QString AlgorithmToString(Algorithm algorithm);
    Algorithm AlgorithmFromString(const QString &name, bool *ok = nullptr);
    QString TypeToString(Type type);
    Type TypeFromString(const QString &name, bool *ok = nullptr);

    QByteArray Base32Decode(const QString &input, bool *ok = nullptr);
    QString Base32Encode(const QByteArray &input);

    QString NormalizeSecret(const QString &secret);

    // Empty means usable; a non-empty result is the translated reason it is not.
    QString Validate(const Entry &entry);

    QString GenerateHOTP(const QByteArray &key, quint64 counter, Algorithm algorithm, int digits);

    // `unixTime` < 0 means now.
    QString GenerateCode(const Entry &entry, qint64 unixTime = -1);

    int SecondsRemaining(const Entry &entry, qint64 unixTime = -1);

    QString ExportToLink(const Entry &entry);
    // An otpauth link with a value that would give wrong codes is refused, and `problem` says why.
    bool ParseFromLink(const QString &link, Entry &out, QString *problem = nullptr);

    // Google Authenticator's format: it pins the step to 30 s, so other periods do not survive.
    QString ExportToMigrationLink(const QList<Entry> &entries);
    // Unusable entries are skipped with a line in `problems`; false when the payload is unreadable or nothing is left.
    bool ParseFromMigrationLink(const QString &link, QList<Entry> &out, QStringList *problems = nullptr);

    // A bare base32 secret yields an unnamed entry.
    QList<Entry> ParseAny(const QString &text, QStringList *problems = nullptr);

    QByteArray ExportToJson(const QList<Entry> &entries);
    bool ParseFromJson(const QByteArray &json, QList<Entry> &out, QStringList *problems = nullptr);
} // namespace OTP
