// DO NOT INCLUDE THIS
#pragma once

#include <functional>
#include <memory>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QDebug>
#include <QApplication>
#include <QStyle>
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
#include <QStyleHints>
#endif
//

// OS
enum osType
{
    unknown = 0,
    Linux = 1,
    Windows = 2,
    Darwin = 3,
};

inline osType getOS()
{
#ifdef Q_OS_MACOS
    return Darwin;
#endif
#ifdef Q_OS_LINUX
    return Linux;
#endif
#ifdef Q_OS_WIN
    return Windows;
#endif
    return unknown;
}

inline QString getOSString() {
    auto os = getOS();
    if (os == Linux) {
        return "Linux";
    }
    if (os == Darwin) {
        return "Darwin";
    }
    if (os == Windows) {
        return "Windows";
    }
    if (os == unknown) {
        return "Unknown";
    }
    return "Unknown";
}

inline QString software_name;
inline QString software_core_name;

// MainWindow functions
class QWidget;
inline QWidget *mainwindow;
inline std::function<void(QString)> MW_show_log;

// Commands delivered to the main window from anywhere in the app via
// MW_dialog_message. The accompanying QStringList carries command-specific
// arguments; recognized tokens live in the MwArg namespace below.
enum class MwMessage {
    UpdateSettings,       // settings were edited; args: MwArg settings-change tokens
    RestartProgram,       // restart the whole application
    Raise,                // bring the main window to the front
    UpdateShortcuts,      // global hotkeys were reconfigured
    ProfileChanged,       // a profile was saved; arg MwArg::RestartProxy when it is running
    GroupsChanged,        // groups were added/removed/edited; refresh the group tabs
    SubscriptionFinished, // a subscription import finished; arg MwArg::Quiet to skip the count line
    SubscriptionNewGroup, // the updater created a new group
    CoreCrashed,          // the external core process died
    CoreStarted,          // the core came up; args: { startedProfileId }
};

// String tokens carried in a MwMessage's argument list.
namespace MwArg {
    // UpdateSettings: which settings changed. Each controls what gets reloaded
    // and whether restarting the proxy/tun/program is offered.
    inline const QString Route        = QStringLiteral("route");
    inline const QString Vpn          = QStringLiteral("vpn");
    inline const QString NeedRestart  = QStringLiteral("needRestart");
    inline const QString ChoosePort   = QStringLiteral("choosePort");
    inline const QString DisableTray  = QStringLiteral("disableTray");
    inline const QString SystemDns    = QStringLiteral("systemDns");
    inline const QString TrayIcon     = QStringLiteral("trayIcon");
    inline const QString MaxLogLines  = QStringLiteral("maxLogLines");
    inline const QString DisableAdmin = QStringLiteral("disableAdmin");
    // ProfileChanged: the saved profile is the running one, so offer a proxy restart.
    inline const QString RestartProxy = QStringLiteral("restartProxy");
    // SubscriptionFinished: a detailed diff was already logged, so skip the import-count line.
    inline const QString Quiet        = QStringLiteral("quiet");
}

inline std::function<void(MwMessage, QStringList)> MW_dialog_message;
// Handles a "throne://" deeplink. Set by MainWindow; marshals to the UI thread.
inline std::function<void(QString)> MW_handle_deeplink;

// Deeplink plumbing (see Utils.cpp). Delivery channels feed URLs in here; the
// pending buffer covers URLs that arrive before the main window exists.
QString Deeplink_ExtractFromArgs(const QStringList &args);
void Deeplink_Submit(const QString &url);
void Deeplink_FlushPending();

// Dispatchers

class QThread;
inline QThread *DS_cores;
inline QThread *LogThread;

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

QStringList SplitAndTrim(const QString& raw, const QString& seperator, bool keepEmpty = true);

// Base64

QByteArray DecodeB64IfValid(const QString &input, QByteArray::Base64Options options = QByteArray::Base64Option::Base64Encoding);

// URL

class QUrlQuery;

QString GetQueryValue(const QUrlQuery &q, const QString &key, const QString &def = "");

QString GetRandomString(int randomStringLength);

quint64 GetRandomUint64();

// Random 127.x.y.z address from the loopback /8. Used to give each internal
// sing-box <-> xray socks bridge a unique destination so concurrent
// connections don't share one ephemeral-port pool. On macOS only 127.0.0.1
// is bound to lo0 by default (other /8 addresses need an `ifconfig alias`),
// so this falls back to 127.0.0.1 there.
QString GenRandomLoopback();

// JSON

class QJsonObject;
class QJsonArray;

QJsonObject QString2QJsonObject(const QString &jsonString);

QString QJsonObject2QString(const QJsonObject &jsonObject, bool compact);

QJsonArray QListInt2QJsonArray(const QList<int> &list);

QJsonArray QListStr2QJsonArray(const QList<QString> &list);

QList<int> QJsonArray2QListInt(const QJsonArray &arr);

QJsonObject QMapString2QJsonObject(const QMap<QString,QString> &mp);

QList<QString> QListInt2QListString(const QList<int> &list);

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

QList<int> MkManyPorts(int num);

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

void HideWindow(QWidget *w);

//

void runOnUiThread(const std::function<void()> &callback, bool wait = false);

void runOnNewThread(const std::function<void()> &callback, bool wait = false);

void runOnThread(const std::function<void()> &callback, QObject *parent, bool wait = false);

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
