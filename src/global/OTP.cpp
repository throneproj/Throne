#include "include/global/OTP.hpp"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageAuthenticationCode>
#include <QObject>
#include <QRegularExpression>
#include <QStringList>
#include <algorithm>
#include <optional>
#include <QUrl>
#include <QUrlQuery>

namespace OTP {
    namespace {
        constexpr char BASE32_ALPHABET[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";

        constexpr int MIN_BARE_SECRET_BYTES = 10;

        QCryptographicHash::Algorithm toHash(Algorithm algorithm) {
            switch (algorithm) {
                case Algorithm::SHA256: return QCryptographicHash::Sha256;
                case Algorithm::SHA512: return QCryptographicHash::Sha512;
                default: return QCryptographicHash::Sha1;
            }
        }

        void pbWriteVarint(QByteArray &out, quint64 value) {
            do {
                quint8 byte = value & 0x7F;
                value >>= 7;
                if (value != 0) byte |= 0x80;
                out.append(static_cast<char>(byte));
            } while (value != 0);
        }

        void pbWriteTag(QByteArray &out, int field, int wireType) {
            pbWriteVarint(out, (static_cast<quint64>(field) << 3) | static_cast<quint64>(wireType));
        }

        void pbWriteBytes(QByteArray &out, int field, const QByteArray &value) {
            pbWriteTag(out, field, 2);
            pbWriteVarint(out, static_cast<quint64>(value.size()));
            out.append(value);
        }

        void pbWriteUint(QByteArray &out, int field, quint64 value) {
            pbWriteTag(out, field, 0);
            pbWriteVarint(out, value);
        }

        bool pbReadVarint(const QByteArray &in, int &pos, quint64 &value) {
            value = 0;
            int shift = 0;
            while (pos < in.size()) {
                const auto byte = static_cast<quint8>(in[pos++]);
                value |= static_cast<quint64>(byte & 0x7F) << shift;
                if ((byte & 0x80) == 0) return true;
                shift += 7;
                if (shift > 63) return false;
            }
            return false;
        }

        bool pbReadField(const QByteArray &in, int &pos, int &field, int &wireType, quint64 &varint, QByteArray &bytes) {
            quint64 tag = 0;
            if (!pbReadVarint(in, pos, tag)) return false;
            field = static_cast<int>(tag >> 3);
            wireType = static_cast<int>(tag & 0x07);
            varint = 0;
            bytes.clear();
            switch (wireType) {
                case 0:
                    return pbReadVarint(in, pos, varint);
                case 1:
                    if (pos + 8 > in.size()) return false;
                    pos += 8;
                    return true;
                case 2: {
                    quint64 len = 0;
                    if (!pbReadVarint(in, pos, len)) return false;
                    if (len > static_cast<quint64>(in.size() - pos)) return false;
                    bytes = in.mid(pos, static_cast<int>(len));
                    pos += static_cast<int>(len);
                    return true;
                }
                case 5:
                    if (pos + 4 > in.size()) return false;
                    pos += 4;
                    return true;
                default:
                    return false;
            }
        }

        // Google's enum: 0 unspecified, 1 SHA1, 2 SHA256, 3 SHA512, 4 MD5.
        std::optional<Algorithm> migrationAlgorithm(quint64 value) {
            switch (value) {
                case 0:
                case 1: return Algorithm::SHA1;
                case 2: return Algorithm::SHA256;
                case 3: return Algorithm::SHA512;
                default: return std::nullopt;
            }
        }

        QString otpEntryLabel(const Entry &entry) {
            if (!entry.name.isEmpty()) return entry.name;
            if (!entry.issuer.isEmpty()) return entry.issuer;
            return QCoreApplication::translate("Validation", "Unnamed entry");
        }

        QString otpBadAlgorithm(const Entry &entry, const QString &value) {
            return QCoreApplication::translate("Validation", "%1: algorithm \"%2\" is not supported (only SHA1, SHA256 and SHA512), so its codes would be wrong.")
                .arg(otpEntryLabel(entry), value);
        }

        QString otpBadDigits(const Entry &entry, const QString &value) {
            return QCoreApplication::translate("Validation", "%1: %2 digits are not supported (use %3 to %4).")
                .arg(otpEntryLabel(entry), value, QString::number(MIN_DIGITS), QString::number(MAX_DIGITS));
        }

        QString otpBadPeriod(const Entry &entry, const QString &value) {
            return QCoreApplication::translate("Validation", "%1: a period of \"%2\" seconds is not supported (use %3 to %4).")
                .arg(otpEntryLabel(entry), value, QString::number(MIN_PERIOD), QString::number(MAX_PERIOD));
        }

        QString otpBadCounter(const Entry &entry, const QString &value) {
            return QCoreApplication::translate("Validation", "%1: the counter \"%2\" is not a whole number of 0 or more.").arg(otpEntryLabel(entry), value);
        }

        QString otpBadType(const Entry &entry, const QString &value) {
            return QCoreApplication::translate("Validation", "%1: type \"%2\" is not supported (only totp and hotp).").arg(otpEntryLabel(entry), value);
        }

        QString otpNoSecret(const Entry &entry) {
            return QCoreApplication::translate("Validation", "%1: there is no secret.").arg(otpEntryLabel(entry));
        }

        QString otpBadSecret(const Entry &entry) {
            return QCoreApplication::translate("Validation", "%1: the secret is not valid base32 (letters A-Z and digits 2-7).").arg(otpEntryLabel(entry));
        }

        // A number or a numeric string, as exports from other apps write either.
        std::optional<qint64> otpJsonInteger(const QJsonValue &value) {
            if (value.isString()) {
                bool ok = false;
                const auto parsed = value.toString().trimmed().toLongLong(&ok);
                return ok ? std::optional<qint64>(parsed) : std::nullopt;
            }
            if (!value.isDouble()) return std::nullopt;
            const double number = value.toDouble();
            if (number != static_cast<double>(static_cast<qint64>(number))) return std::nullopt;
            return static_cast<qint64>(number);
        }

        QString otpJsonText(const QJsonValue &value) {
            if (value.isString()) return value.toString();
            if (value.isDouble()) return QString::number(value.toDouble());
            return QString::fromUtf8(QJsonDocument(QJsonArray{value}).toJson(QJsonDocument::Compact)).mid(1).chopped(1).left(40);
        }

        quint64 migrationAlgorithmValue(Algorithm algorithm) {
            switch (algorithm) {
                case Algorithm::SHA256: return 2;
                case Algorithm::SHA512: return 3;
                default: return 1;
            }
        }

        // Even grouping is what separates a printed secret from prose, which is also all base32 letters.
        bool LooksLikeBareSecret(const QString &line) {
            const auto groups = line.split(QRegularExpression("[\\s-]+"), Qt::SkipEmptyParts);
            if (groups.isEmpty()) return false;
            if (groups.size() == 1) return true;
            const auto width = groups.first().size();
            if (width > 8) return false;
            return std::all_of(groups.begin(), groups.end(),
                               [width](const QString &group) { return group.size() == width; });
        }

        // Empty `problem` means the entry is usable.
        Entry entryFromJsonObject(const QJsonObject &object, QString &problem) {
            Entry entry;
            entry.name = object["name"].toString();
            entry.issuer = object["issuer"].toString();
            const auto secret = object["secret"].toString();
            if (secret.trimmed().isEmpty()) {
                problem = otpNoSecret(entry);
                return entry;
            }
            entry.secret = NormalizeSecret(secret);
            if (entry.secret.isEmpty()) {
                problem = otpBadSecret(entry);
                return entry;
            }
            bool ok = false;
            const auto algorithm = object["algorithm"].toString();
            entry.algorithm = AlgorithmFromString(algorithm, &ok);
            if (!ok) {
                problem = otpBadAlgorithm(entry, algorithm);
                return entry;
            }
            const auto type = object["type"].toString();
            entry.type = TypeFromString(type, &ok);
            if (!ok) {
                problem = otpBadType(entry, type);
                return entry;
            }
            if (object.contains("digits")) {
                const auto digits = otpJsonInteger(object["digits"]);
                if (!digits || *digits < MIN_DIGITS || *digits > MAX_DIGITS) {
                    problem = otpBadDigits(entry, otpJsonText(object["digits"]));
                    return entry;
                }
                entry.digits = static_cast<int>(*digits);
            }
            if (entry.type == Type::TOTP && object.contains("period")) {
                const auto period = otpJsonInteger(object["period"]);
                if (!period || *period < MIN_PERIOD || *period > MAX_PERIOD) {
                    problem = otpBadPeriod(entry, otpJsonText(object["period"]));
                    return entry;
                }
                entry.period = static_cast<int>(*period);
            }
            if (entry.type == Type::HOTP && object.contains("counter")) {
                const auto counter = otpJsonInteger(object["counter"]);
                if (!counter || *counter < 0) {
                    problem = otpBadCounter(entry, otpJsonText(object["counter"]));
                    return entry;
                }
                entry.counter = *counter;
            }
            return entry;
        }

        QJsonObject entryToJsonObject(const Entry &entry) {
            QJsonObject object;
            object["name"] = entry.name;
            if (!entry.issuer.isEmpty()) object["issuer"] = entry.issuer;
            object["secret"] = entry.secret;
            object["algorithm"] = AlgorithmToString(entry.algorithm);
            object["type"] = TypeToString(entry.type);
            object["digits"] = entry.digits;
            if (entry.type == Type::TOTP) object["period"] = entry.period;
            else object["counter"] = entry.counter;
            return object;
        }
    }

    QString AlgorithmToString(Algorithm algorithm) {
        switch (algorithm) {
            case Algorithm::SHA256: return "SHA256";
            case Algorithm::SHA512: return "SHA512";
            default: return "SHA1";
        }
    }

    Algorithm AlgorithmFromString(const QString &name, bool *ok) {
        if (ok) *ok = true;
        const auto normalized = name.trimmed().toUpper().remove(QLatin1Char('-'));
        if (normalized == "SHA256") return Algorithm::SHA256;
        if (normalized == "SHA512") return Algorithm::SHA512;
        if (normalized == "SHA1" || normalized.isEmpty()) return Algorithm::SHA1;
        if (ok) *ok = false;
        return Algorithm::SHA1;
    }

    QString TypeToString(Type type) {
        return type == Type::HOTP ? "hotp" : "totp";
    }

    Type TypeFromString(const QString &name, bool *ok) {
        if (ok) *ok = true;
        const auto normalized = name.trimmed().toLower();
        if (normalized == "hotp") return Type::HOTP;
        if (normalized == "totp" || normalized.isEmpty()) return Type::TOTP;
        if (ok) *ok = false;
        return Type::TOTP;
    }

    QByteArray Base32Decode(const QString &input, bool *ok) {
        if (ok) *ok = false;
        QByteArray out;
        quint32 buffer = 0;
        int bitsLeft = 0;
        for (const QChar &qc : input) {
            const char c = qc.toLatin1();
            if (c == ' ' || c == '-' || c == '_' || c == '\t' || c == '\r' || c == '\n' || c == '=') continue;
            int value;
            if (c >= 'A' && c <= 'Z') value = c - 'A';
            else if (c >= 'a' && c <= 'z') value = c - 'a';
            else if (c >= '2' && c <= '7') value = c - '2' + 26;
            else return {};
            buffer = (buffer << 5) | static_cast<quint32>(value);
            bitsLeft += 5;
            if (bitsLeft >= 8) {
                bitsLeft -= 8;
                out.append(static_cast<char>((buffer >> bitsLeft) & 0xFF));
            }
        }
        if (ok) *ok = true;
        return out;
    }

    QString Base32Encode(const QByteArray &input) {
        QString out;
        quint32 buffer = 0;
        int bitsLeft = 0;
        for (const char c : input) {
            buffer = (buffer << 8) | static_cast<quint8>(c);
            bitsLeft += 8;
            while (bitsLeft >= 5) {
                bitsLeft -= 5;
                out.append(QLatin1Char(BASE32_ALPHABET[(buffer >> bitsLeft) & 0x1F]));
            }
        }
        if (bitsLeft > 0) out.append(QLatin1Char(BASE32_ALPHABET[(buffer << (5 - bitsLeft)) & 0x1F]));
        return out;
    }

    QString NormalizeSecret(const QString &secret) {
        bool ok = false;
        const auto decoded = Base32Decode(secret, &ok);
        if (!ok || decoded.isEmpty()) return {};
        return Base32Encode(decoded);
    }

    QString Validate(const Entry &entry) {
        if (entry.secret.trimmed().isEmpty()) return QObject::tr("Secret is empty");
        bool ok = false;
        const auto key = Base32Decode(entry.secret, &ok);
        if (!ok) return QObject::tr("Secret is not valid base32");
        if (key.isEmpty()) return QObject::tr("Secret is too short");
        if (entry.digits < MIN_DIGITS || entry.digits > MAX_DIGITS)
            return QObject::tr("Digits must be between %1 and %2").arg(MIN_DIGITS).arg(MAX_DIGITS);
        if (entry.type == Type::TOTP && (entry.period < MIN_PERIOD || entry.period > MAX_PERIOD))
            return QObject::tr("Period must be between %1 and %2 seconds").arg(MIN_PERIOD).arg(MAX_PERIOD);
        return {};
    }

    QString GenerateHOTP(const QByteArray &key, quint64 counter, Algorithm algorithm, int digits) {
        if (key.isEmpty() || digits < MIN_DIGITS || digits > MAX_DIGITS) return {};

        QByteArray message(8, Qt::Uninitialized);
        for (int i = 7; i >= 0; --i) {
            message[i] = static_cast<char>(counter & 0xFF);
            counter >>= 8;
        }

        const auto mac = QMessageAuthenticationCode::hash(message, key, toHash(algorithm));
        if (mac.size() < 20) return {};

        // RFC 4226 dynamic truncation: last nibble picks the window, top bit masked off.
        const int offset = mac[mac.size() - 1] & 0x0F;
        const quint32 binary = (static_cast<quint32>(static_cast<quint8>(mac[offset])) & 0x7F) << 24
                             | static_cast<quint32>(static_cast<quint8>(mac[offset + 1])) << 16
                             | static_cast<quint32>(static_cast<quint8>(mac[offset + 2])) << 8
                             | static_cast<quint32>(static_cast<quint8>(mac[offset + 3]));

        quint64 modulus = 1;
        for (int i = 0; i < digits; ++i) modulus *= 10;
        return QString("%1").arg(binary % modulus, digits, 10, QLatin1Char('0'));
    }

    QString GenerateCode(const Entry &entry, qint64 unixTime) {
        if (!Validate(entry).isEmpty()) return {};
        const auto key = Base32Decode(entry.secret);
        if (entry.type == Type::HOTP) {
            return GenerateHOTP(key, static_cast<quint64>(entry.counter), entry.algorithm, entry.digits);
        }
        const qint64 now = unixTime < 0 ? QDateTime::currentSecsSinceEpoch() : unixTime;
        return GenerateHOTP(key, static_cast<quint64>(now / entry.period), entry.algorithm, entry.digits);
    }

    int SecondsRemaining(const Entry &entry, qint64 unixTime) {
        if (entry.type == Type::HOTP || entry.period < MIN_PERIOD) return 0;
        const qint64 now = unixTime < 0 ? QDateTime::currentSecsSinceEpoch() : unixTime;
        return static_cast<int>(entry.period - now % entry.period);
    }

    QString ExportToLink(const Entry &entry) {
        QUrl url;
        url.setScheme("otpauth");
        url.setHost(TypeToString(entry.type));
        url.setPath("/" + (entry.issuer.isEmpty() ? entry.name : entry.issuer + ":" + entry.name));

        QUrlQuery query;
        query.addQueryItem("secret", entry.secret);
        if (!entry.issuer.isEmpty()) query.addQueryItem("issuer", entry.issuer);
        query.addQueryItem("algorithm", AlgorithmToString(entry.algorithm));
        query.addQueryItem("digits", QString::number(entry.digits));
        if (entry.type == Type::HOTP) query.addQueryItem("counter", QString::number(entry.counter));
        else query.addQueryItem("period", QString::number(entry.period));
        url.setQuery(query);

        return url.toString(QUrl::FullyEncoded);
    }

    bool ParseFromLink(const QString &link, Entry &out, QString *problem) {
        const QUrl url(link.trimmed());
        if (!url.isValid() || url.scheme().compare("otpauth", Qt::CaseInsensitive) != 0) return false;
        const auto fail = [problem](const QString &reason) {
            if (problem != nullptr) *problem = reason;
            return false;
        };

        Entry entry;
        auto label = url.path();
        if (label.startsWith('/')) label.remove(0, 1);
        if (const int sep = label.indexOf(':'); sep >= 0) {
            entry.issuer = label.left(sep).trimmed();
            entry.name = label.mid(sep + 1).trimmed();
        } else {
            entry.name = label.trimmed();
        }

        const QUrlQuery query(url);
        if (const auto issuer = query.queryItemValue("issuer", QUrl::FullyDecoded).trimmed(); !issuer.isEmpty())
            entry.issuer = issuer;
        if (entry.name.isEmpty()) entry.name = entry.issuer;

        bool ok = false;
        entry.type = TypeFromString(url.host(), &ok);
        if (!ok) return fail(otpBadType(entry, url.host()));
        const auto secret = query.queryItemValue("secret", QUrl::FullyDecoded);
        if (secret.trimmed().isEmpty()) return fail(otpNoSecret(entry));
        entry.secret = NormalizeSecret(secret);
        if (entry.secret.isEmpty()) return fail(otpBadSecret(entry));

        if (query.hasQueryItem("algorithm")) {
            const auto algorithm = query.queryItemValue("algorithm", QUrl::FullyDecoded);
            entry.algorithm = AlgorithmFromString(algorithm, &ok);
            if (!ok) return fail(otpBadAlgorithm(entry, algorithm));
        }
        if (query.hasQueryItem("digits")) {
            const auto digits = query.queryItemValue("digits", QUrl::FullyDecoded).trimmed();
            entry.digits = digits.toInt(&ok);
            if (!ok || entry.digits < MIN_DIGITS || entry.digits > MAX_DIGITS) return fail(otpBadDigits(entry, digits));
        }
        // period applies only to TOTP and counter only to HOTP; the other type's parameter is ignored.
        if (entry.type == Type::TOTP && query.hasQueryItem("period")) {
            const auto period = query.queryItemValue("period", QUrl::FullyDecoded).trimmed();
            entry.period = period.toInt(&ok);
            if (!ok || entry.period < MIN_PERIOD || entry.period > MAX_PERIOD) return fail(otpBadPeriod(entry, period));
        }
        if (entry.type == Type::HOTP && query.hasQueryItem("counter")) {
            const auto counter = query.queryItemValue("counter", QUrl::FullyDecoded).trimmed();
            entry.counter = counter.toLongLong(&ok);
            if (!ok || entry.counter < 0) return fail(otpBadCounter(entry, counter));
        }

        out = entry;
        return true;
    }

    QString ExportToMigrationLink(const QList<Entry> &entries) {
        QByteArray payload;
        for (const auto &entry : entries) {
            bool ok = false;
            const auto key = Base32Decode(entry.secret, &ok);
            if (!ok || key.isEmpty()) continue;

            QByteArray parameters;
            pbWriteBytes(parameters, 1, key);
            pbWriteBytes(parameters, 2, entry.name.toUtf8());
            if (!entry.issuer.isEmpty()) pbWriteBytes(parameters, 3, entry.issuer.toUtf8());
            pbWriteUint(parameters, 4, migrationAlgorithmValue(entry.algorithm));
            pbWriteUint(parameters, 5, entry.digits == 8 ? 2 : 1);
            pbWriteUint(parameters, 6, entry.type == Type::HOTP ? 1 : 2);
            if (entry.type == Type::HOTP) pbWriteUint(parameters, 7, static_cast<quint64>(entry.counter));

            pbWriteBytes(payload, 1, parameters);
        }
        if (payload.isEmpty()) return {};

        pbWriteUint(payload, 2, 1); // version
        pbWriteUint(payload, 3, 1); // batch_size
        pbWriteUint(payload, 4, 0); // batch_index

        QUrl url;
        url.setScheme("otpauth-migration");
        url.setHost("offline");
        QUrlQuery query;
        query.addQueryItem("data", QString::fromUtf8(payload.toBase64()));
        url.setQuery(query);
        return url.toString(QUrl::FullyEncoded);
    }

    bool ParseFromMigrationLink(const QString &link, QList<Entry> &out, QStringList *problems) {
        const QUrl url(link.trimmed());
        if (!url.isValid() || url.scheme().compare("otpauth-migration", Qt::CaseInsensitive) != 0) return false;

        const auto data = QUrlQuery(url).queryItemValue("data", QUrl::FullyDecoded);
        if (data.isEmpty()) return false;
        const auto payload = QByteArray::fromBase64(data.toUtf8());
        if (payload.isEmpty()) return false;

        QList<Entry> entries;
        QStringList skipped;
        int pos = 0;
        while (pos < payload.size()) {
            int field = 0, wireType = 0;
            quint64 varint = 0;
            QByteArray bytes;
            if (!pbReadField(payload, pos, field, wireType, varint, bytes)) return false;
            if (field != 1 || wireType != 2) continue;

            Entry entry;
            bool haveSecret = false;
            std::optional<quint64> badAlgorithm, badDigits, badType;
            int inner = 0;
            while (inner < bytes.size()) {
                int f = 0, w = 0;
                quint64 v = 0;
                QByteArray b;
                if (!pbReadField(bytes, inner, f, w, v, b)) return false;
                switch (f) {
                    case 1: entry.secret = Base32Encode(b); haveSecret = !b.isEmpty(); break;
                    case 2: entry.name = QString::fromUtf8(b); break;
                    case 3: entry.issuer = QString::fromUtf8(b); break;
                    case 4:
                        if (const auto algorithm = migrationAlgorithm(v)) entry.algorithm = *algorithm;
                        else badAlgorithm = v;
                        break;
                    // 0 unspecified, 1 six, 2 eight.
                    case 5:
                        if (v <= 2) entry.digits = v == 2 ? 8 : DEFAULT_DIGITS;
                        else badDigits = v;
                        break;
                    // 0 unspecified, 1 HOTP, 2 TOTP.
                    case 6:
                        if (v <= 2) entry.type = v == 1 ? Type::HOTP : Type::TOTP;
                        else badType = v;
                        break;
                    case 7: entry.counter = static_cast<qint64>(v); break;
                    default: break;
                }
            }
            if (entry.name.isEmpty()) entry.name = entry.issuer;
            if (!haveSecret) skipped << otpNoSecret(entry);
            else if (badAlgorithm) skipped << otpBadAlgorithm(entry, *badAlgorithm == 4 ? QStringLiteral("MD5") : QString::number(*badAlgorithm));
            else if (badDigits) skipped << QCoreApplication::translate("Validation", "%1: the digit count code %2 is not supported.").arg(otpEntryLabel(entry), QString::number(*badDigits));
            else if (badType) skipped << otpBadType(entry, QString::number(*badType));
            else entries.append(entry);
        }

        if (problems != nullptr) *problems += skipped;
        if (entries.isEmpty()) return false;
        out = entries;
        return true;
    }

    QByteArray ExportToJson(const QList<Entry> &entries) {
        QJsonArray array;
        for (const auto &entry : entries) array.append(entryToJsonObject(entry));
        QJsonObject root;
        root["version"] = 1;
        root["otp"] = array;
        return QJsonDocument(root).toJson(QJsonDocument::Indented);
    }

    bool ParseFromJson(const QByteArray &json, QList<Entry> &out, QStringList *problems) {
        QJsonParseError error{};
        const auto document = QJsonDocument::fromJson(json, &error);
        if (error.error != QJsonParseError::NoError) return false;

        QJsonArray array;
        if (document.isArray()) array = document.array();
        else if (document.isObject()) array = document.object()["otp"].toArray();
        if (array.isEmpty()) return false;

        QList<Entry> entries;
        for (const auto &value : array) {
            QString problem;
            if (!value.isObject()) {
                problem = QCoreApplication::translate("Validation", "Skipped an item that is not an OTP entry.");
            } else if (auto entry = entryFromJsonObject(value.toObject(), problem); problem.isEmpty()) {
                entries.append(entry);
                continue;
            }
            if (problems != nullptr) *problems << problem;
        }

        if (entries.isEmpty()) return false;
        out = entries;
        return true;
    }

    QList<Entry> ParseAny(const QString &text, QStringList *problems) {
        QList<Entry> result;
        const auto trimmed = text.trimmed();
        if (trimmed.isEmpty()) return result;

        if (trimmed.startsWith('{') || trimmed.startsWith('[')) {
            QStringList skipped;
            const bool parsed = ParseFromJson(trimmed.toUtf8(), result, &skipped);
            if (problems) {
                if (!parsed && skipped.isEmpty()) *problems << QObject::tr("Not a readable OTP export");
                *problems += skipped;
            }
            return result;
        }

        const auto lines = trimmed.split(QRegularExpression("[\r\n]+"), Qt::SkipEmptyParts);
        for (const auto &rawLine : lines) {
            const auto line = rawLine.trimmed();
            if (line.isEmpty()) continue;

            if (line.startsWith("otpauth-migration://", Qt::CaseInsensitive)) {
                QList<Entry> batch;
                QStringList skipped;
                if (ParseFromMigrationLink(line, batch, &skipped)) result += batch;
                else if (problems && skipped.isEmpty()) *problems << QObject::tr("Unreadable migration link");
                if (problems) *problems += skipped;
                continue;
            }

            Entry entry;
            QString problem;
            if (ParseFromLink(line, entry, &problem)) {
                result.append(entry);
                continue;
            }
            // Never echo an otpauth line: it holds the secret.
            if (!problem.isEmpty() || line.startsWith("otpauth://", Qt::CaseInsensitive)) {
                if (problems) *problems << (problem.isEmpty() ? QCoreApplication::translate("Validation", "An otpauth link could not be read.") : problem);
                continue;
            }

            if (const auto secret = LooksLikeBareSecret(line) ? NormalizeSecret(line) : QString(); !secret.isEmpty()) {
                if (Base32Decode(secret).size() >= MIN_BARE_SECRET_BYTES) {
                    Entry bare;
                    bare.secret = secret;
                    result.append(bare);
                } else if (problems) {
                    *problems << QCoreApplication::translate("Validation", "Skipped a base32 secret shorter than %1 characters.")
                                     .arg((MIN_BARE_SECRET_BYTES * 8 + 4) / 5);
                }
                continue;
            }

            // A lone token may be a mistyped secret, so only prose is echoed.
            if (problems) {
                *problems << (LooksLikeBareSecret(line) ? QCoreApplication::translate("Validation", "Skipped a secret that is not valid base32 (letters A-Z and digits 2-7).")
                                                        : QObject::tr("Not an otpauth link or secret: %1").arg(line.left(40)));
            }
        }
        return result;
    }
} // namespace OTP
