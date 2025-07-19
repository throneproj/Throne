// DO NOT INCLUDE THIS
#pragma once

#include <functional>
#include <memory>
#include <QObject>
#include <QString>
#include <QDebug>
#include <QApplication>
#include <QStyle>
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
#include <QStyleHints>
#endif
//

inline QString software_name;
inline QString software_core_name;

// MainWindow functions
class QWidget;
inline QWidget *mainwindow;
inline std::function<void(QString)> MW_show_log;
inline std::function<void(QString, QString)> MW_dialog_message;

// Dispatchers

class QThread;
inline QThread *DS_cores;

// Timers

class QTimer;
inline QTimer *TM_auto_update_subsctiption;
inline std::function<void(int)> TM_auto_update_subsctiption_Reset_Minute;

// String

#define FIRST_OR_SECOND(a, b) a.isEmpty() ? b : a

inline const QString UNICODE_LRO = QString::fromUtf8(QByteArray::fromHex("E280AD"));

#define Int2String(num) QString::number(num)

inline QString SubStrBefore(QString str, const QString &sub) {
    if (!str.contains(sub)) return str;
    return str.left(str.indexOf(sub));
}

inline QString SubStrAfter(QString str, const QString &sub) {
    if (!str.contains(sub)) return str;
    return str.right(str.length() - str.indexOf(sub) - sub.length());
}

QString QStringList2Command(const QStringList &list);

QStringList SplitLines(const QString &_string);

QStringList SplitLinesSkipSharp(const QString &_string, int maxLine = 0);

// Base64

QByteArray DecodeB64IfValid(const QString &input, QByteArray::Base64Options options = QByteArray::Base64Option::Base64Encoding);

// URL

class QUrlQuery;

#define GetQuery(url) QUrlQuery((url).query(QUrl::ComponentFormattingOption::FullyDecoded));

QString GetQueryValue(const QUrlQuery &q, const QString &key, const QString &def = "");

QString GetRandomString(int randomStringLength);

quint64 GetRandomUint64();

// JSON

class QJsonObject;
class QJsonArray;

QJsonObject QString2QJsonObject(const QString &jsonString);

QString QJsonObject2QString(const QJsonObject &jsonObject, bool compact);

QJsonArray QListInt2QJsonArray(const QList<int> &list);

QJsonArray QListStr2QJsonArray(const QList<QString> &list);

QList<int> QJsonArray2QListInt(const QJsonArray &arr);

QJsonObject QMapString2QJsonObject(const QMap<QString,QString> &mp);

#define QJSONARRAY_ADD(arr, add) \
    for (const auto &a: (add)) { \
        (arr) += a;              \
    }
#define QJSONOBJECT_COPY(src, dst, key) \
    if (src.contains(key)) dst[key] = src[key];
#define QJSONOBJECT_COPY2(src, dst, src_key, dst_key) \
    if (src.contains(src_key)) dst[dst_key] = src[src_key];

QList<QString> QJsonArray2QListString(const QJsonArray &arr);

QJsonArray QString2QJsonArray(const QString& str);

// Files

QByteArray ReadFile(const QString &path);

QString ReadFileText(const QString &path);

// Validators

bool IsIpAddress(const QString &str);

bool IsIpAddressV4(const QString &str);

bool IsIpAddressV6(const QString &str);

// [2001:4860:4860::8888] -> 2001:4860:4860::8888
inline QString UnwrapIPV6Host(QString &str) {
    return str.replace("[", "").replace("]", "");
}

// [2001:4860:4860::8888] or 2001:4860:4860::8888 -> [2001:4860:4860::8888]
inline QString WrapIPV6Host(QString &str) {
    if (!IsIpAddressV6(str)) return str;
    return "[" + UnwrapIPV6Host(str) + "]";
}

inline QString DisplayAddress(QString serverAddress, int serverPort) {
    if (serverAddress.isEmpty() && serverPort == 0) return {};
    return WrapIPV6Host(serverAddress) + ":" + Int2String(serverPort);
}

inline QString DisplayDest(const QString& dest, QString domain)
{
    if (domain.isEmpty() || dest.split(":").first() == domain) return dest;
    return dest + " (" + domain + ")";
}

// Format & Misc

int MkPort();

QString DisplayTime(long long time, int formatType = 0);

QString ReadableSize(const qint64 &size);

inline bool InRange(unsigned x, unsigned low, unsigned high) {
    return (low <= x && x <= high);
}

inline bool IsValidPort(int port) {
    return InRange(port, 1, 65535);
}

// UI

QWidget *GetMessageBoxParent();

int MessageBoxWarning(const QString &title, const QString &text);

int MessageBoxInfo(const QString &title, const QString &text);

void ActivateWindow(QWidget *w);

//

void runOnUiThread(const std::function<void()> &callback, QObject *parent = nullptr);

void runOnNewThread(const std::function<void()> &callback);

void runOnThread(const std::function<void()> &callback, QThread *thread);

template<typename EMITTER, typename SIGNAL, typename RECEIVER, typename ReceiverFunc>
inline void connectOnce(EMITTER *emitter, SIGNAL signal, RECEIVER *receiver, ReceiverFunc f,
                        Qt::ConnectionType connectionType = Qt::AutoConnection) {
    auto connection = std::make_shared<QMetaObject::Connection>();
    auto onTriggered = [connection, f](auto... arguments) {
        std::invoke(f, arguments...);
        QObject::disconnect(*connection);
    };

    *connection = QObject::connect(emitter, signal, receiver, onTriggered, connectionType);
}

void setTimeout(const std::function<void()> &callback, QObject *obj, int timeout = 0);

inline bool isDarkMode() {
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    return qApp->styleHints()->colorScheme() == Qt::ColorScheme::Dark;
#endif
    return qApp->style()->standardPalette().window().color().lightness() < qApp->style()->standardPalette().windowText().color().lightness();
}
