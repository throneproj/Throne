#include "include/global/Utils.hpp"

#include "3rdparty/QThreadCreateThread.hpp"

#include <random>

#include <QApplication>
#include <QUrlQuery>
#include <QTcpServer>
#include <QTimer>
#include <QMessageBox>
#include <QFile>
#include <QFileInfo>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QDateTime>
#include <QLocale>
#include <QCheckBox>
#include <QLayout>
#include <QVBoxLayout>
#include <QPlainTextEdit>
#include <QDialogButtonBox>
#include <QDialog>

#ifdef Q_OS_WIN
#include "include/sys/windows/guihelper.h"
#endif
#ifdef Q_OS_MAC
#include <ApplicationServices/ApplicationServices.h>
#endif

QStringList SplitLines(const QString &_string) {
    static const QRegularExpression lineSplitRegex("[\r\n]");
    return _string.split(lineSplitRegex, Qt::SplitBehaviorFlags::SkipEmptyParts);
}

QByteArray DecodeB64IfValid(const QString &input, QByteArray::Base64Options options) {
    QByteArray::Base64Options newOptions = options | QByteArray::Base64Option::AbortOnBase64DecodingErrors;
    auto result = QByteArray::fromBase64Encoding(input.toUtf8(), newOptions);
    if (result) {
        return result.decoded;
    }
    return {};
}

QStringList SplitAndTrim(const QString& raw, const QString& separator, bool keepEmpty) {
    QStringList result;
    auto spl = raw.split(separator);
    for (const auto& str : spl) {
        auto trimmed = str.trimmed();
        if (!keepEmpty && trimmed.isEmpty()) continue;
        result << trimmed;
    }
    return result;
}

QString QStringList2Command(const QStringList &list) {
    QStringList new_list;
    for (auto str: list) {
        auto q = "\"" + str.replace("\"", "\\\"") + "\"";
        new_list << q;
    }
    return new_list.join(" ");
}

QString GetQueryValue(const QUrlQuery &q, const QString &key, const QString &def) {
    auto a = q.queryItemValue(key);
    if (a.isEmpty()) {
        return def;
    }
    return a;
}

QString GetRandomString(int randomStringLength) {
    std::random_device rd;
    std::mt19937 mt(rd());

    const QString possibleCharacters("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789");

    std::uniform_int_distribution<int> dist(0, possibleCharacters.length() - 1);

    QString randomString;
    for (int i = 0; i < randomStringLength; ++i) {
        QChar nextChar = possibleCharacters.at(dist(mt));
        randomString.append(nextChar);
    }
    return randomString;
}

quint64 GetRandomUint64() {
    std::random_device rd;
    std::mt19937 mt(rd());
    std::uniform_int_distribution<quint64> dist;
    return dist(mt);
}

// QString >> QJson
QJsonObject QString2QJsonObject(const QString &jsonString) {
    QJsonDocument jsonDocument = QJsonDocument::fromJson(jsonString.toUtf8());
    QJsonObject jsonObject = jsonDocument.object();
    return jsonObject;
}

// QJson >> QString
QString QJsonObject2QString(const QJsonObject &jsonObject, bool compact) {
    return QJsonDocument(jsonObject).toJson(compact ? QJsonDocument::Compact : QJsonDocument::Indented);
}

QJsonArray QListStr2QJsonArray(const QList<QString> &list) {
    QVariantList list2;
    for (const auto &item: list) {
        if (QStringView(item).trimmed().isEmpty()) continue;
        list2.append(item);
    }

    return list2.isEmpty() ? QJsonArray{} : QJsonArray::fromVariantList(list2);
}

QJsonArray QListInt2QJsonArray(const QList<int> &list) {
    QJsonArray arr;
    for (const int item: list)
        arr.append(item);
    return arr;
}

QList<int> QJsonArray2QListInt(const QJsonArray &arr) {
    QList<int> list2;
    list2.reserve(arr.size());
    for (auto item: arr)
        list2.append(item.toInt());
    return list2;
}

QList<QString> QJsonArray2QListString(const QJsonArray &arr) {
    QList<QString> list2;
    for (auto item: arr)
        list2.append(item.toString());
    return list2;
}

QJsonArray QString2QJsonArray(const QString& str) {
    auto doc = QJsonDocument::fromJson(str.toUtf8());
    if (doc.isArray()) {
        return doc.array();
    }
    return {};
}

QJsonObject QMapString2QJsonObject(const QMap<QString,QString> &mp) {
    QJsonObject res;
    for (auto it = mp.cbegin(); it != mp.cend(); ++it) {
        res.insert(it.key(), it.value());
    }
    return res;
}

QList<QString> QListInt2QListString(const QList<int> &list) {
    QList<QString> resp;
    for (int item : list) resp << Int2String(item);
    return resp;
}

QList<int> QStringList2QListInt(const QList<QString> &list) {
    QList<int> resp;
    resp.reserve(list.size());
    for (const auto& item: list) resp.append(item.toInt());
    return resp;
}

QByteArray ReadFile(const QString &path) {
    QFile file(path);
    file.open(QFile::ReadOnly);
    return file.readAll();
}

QString ReadFileText(const QString &path) {
    QFile file(path);
    file.open(QFile::ReadOnly | QFile::Text);
    QTextStream stream(&file);
    return stream.readAll();
}

static bool listenWithRetry(QTcpServer *server, const QString &address) {
    QHostAddress bindAddress;
    const bool anyInterface = address.isEmpty() || !bindAddress.setAddress(address);
    for (int attempt = 0; attempt < 3; attempt++) {
        if (!anyInterface && server->listen(bindAddress)) return true;
        if (anyInterface && (server->listen(QHostAddress::Any) || server->listen(QHostAddress::AnyIPv4))) return true;
    }
    MW_show_log("[Ports] could not reserve a local port on " + (anyInterface ? "any interface" : address) +
                ": " + server->errorString());
    return false;
}

int MkPort(const QString &address) {
    QTcpServer s;
    if (!listenWithRetry(&s, address)) return 0;
    auto port = s.serverPort();
    s.close();
    return port;
}

QList<int> MkManyPorts(int num, const QString &address) {
    QList<int> res;
    QList<QTcpServer*> servers;
    for (int i=0;i<num;i++) {
        auto server = new QTcpServer();
        listenWithRetry(server, address);
        servers.append(server);
        res.append(server->serverPort());
    }
    for (const auto s: servers) {
        s->close();
        delete s;
    }
    servers.clear();
    return res;
}

QString ReadableSize(const qint64 &size) {
    double sizeAsDouble = size;
    static QStringList measures;
    if (measures.isEmpty())
        measures << "B"
                 << "KiB"
                 << "MiB"
                 << "GiB"
                 << "TiB"
                 << "PiB"
                 << "EiB"
                 << "ZiB"
                 << "YiB";
    QStringListIterator it(measures);
    QString measure(it.next());
    while (sizeAsDouble >= 1024.0 && it.hasNext()) {
        measure = it.next();
        sizeAsDouble /= 1024.0;
    }
    return QString::fromLatin1("%1 %2").arg(sizeAsDouble, 0, 'f', 2).arg(measure);
}

bool IsIpAddress(const QString &str) {
    auto address = QHostAddress(str);
    if (address.protocol() == QAbstractSocket::IPv4Protocol || address.protocol() == QAbstractSocket::IPv6Protocol)
        return true;
    return false;
}

bool IsIpAddressV4(const QString &str) {
    return (QHostAddress(str).protocol() == QAbstractSocket::IPv4Protocol);
}

bool IsIpAddressV6(const QString &str) {
    return (QHostAddress(str).protocol() == QAbstractSocket::IPv6Protocol);
}

QString DisplayTime(long long time, int formatType) {
    QDateTime t;
    t.setMSecsSinceEpoch(time * 1000);
    return QLocale().toString(t, QLocale::FormatType(formatType));
}

QWidget *GetMessageBoxParent() {
    auto activeWindow = QApplication::activeWindow();
    if (activeWindow == nullptr && mainwindow != nullptr) {
        if (mainwindow->isVisible()) return mainwindow;
        return nullptr;
    }
    return activeWindow;
}

int MessageBoxWarning(const QString &title, const QString &text) {
    return QMessageBox::warning(GetMessageBoxParent(), title, text);
}

int MessageBoxInfo(const QString &title, const QString &text) {
    return QMessageBox::information(GetMessageBoxParent(), title, text);
}

void MessageBoxScrollable(const QString &title, const QString &text) {
    QDialog dialog(GetMessageBoxParent());
    dialog.setWindowTitle(title);
    auto *layout = new QVBoxLayout(&dialog);
    auto *view = new QPlainTextEdit(&dialog);
    view->setPlainText(text);
    view->setReadOnly(true);
    layout->addWidget(view);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok, &dialog);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    layout->addWidget(buttons);
    dialog.resize(480, 420);
    dialog.exec();
}

int MessageBoxCheck(const QString &title, const QString &text, const QString &checkBoxText, bool &isChecked) {
    QMessageBox msgBox(GetMessageBoxParent());
    msgBox.setWindowTitle(title);
    msgBox.setText(text);
    msgBox.setIcon(QMessageBox::Question);
    msgBox.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
    msgBox.setDefaultButton(QMessageBox::Ok);

    QCheckBox *checkBox = new QCheckBox(checkBoxText);
    checkBox->setChecked(isChecked);

    dynamic_cast< QGridLayout *>(msgBox.layout())->addWidget(checkBox, 1, 2);

    int result = msgBox.exec();

    isChecked = checkBox->isChecked();

    return result;
}

void ActivateWindow(QWidget *w) {
    w->setWindowState(w->windowState() & ~Qt::WindowMinimized);
    w->setVisible(true);
#ifdef Q_OS_WIN
    Windows_QWidget_SetForegroundWindow(w);
#elif defined(Q_OS_MAC)
    ProcessSerialNumber psn = { 0, kCurrentProcess };
    TransformProcessType(&psn, kProcessTransformToForegroundApplication);
#endif
    w->raise();
    w->activateWindow();
}

void HideWindow(QWidget *w) {
    w->hide();
#ifdef Q_OS_MAC
    ProcessSerialNumber psn = { 0, kCurrentProcess };
    TransformProcessType(&psn, kProcessTransformToUIElementApplication);
#endif
}

void runOnUiThread(const std::function<void()> &callback, bool wait) {
    // any thread. Targets qApp's thread rather than mainwindow's: they are the
    // same thread, but qApp exists for the whole of main() while mainwindow stays
    // null until UI_InitMainWindow(). Background work started before that (e.g.
    // the traffic-stats rollup) can report errors through here, and dereferencing
    // the null mainwindow crashed the worker thread.
    auto *app = QCoreApplication::instance();
    if (app == nullptr) return;
    auto thread = app->thread();
    if (thread == QThread::currentThread()) {
        callback();
        return;
    }
    auto *timer = new QTimer();
    timer->moveToThread(thread);
    timer->setSingleShot(true);

    QEventLoop loop;
    QObject::connect(timer, &QTimer::timeout, [=, &loop]() {
        // main thread
        callback();
        timer->deleteLater();

        if (wait)
        {
            QMetaObject::invokeMethod(&loop, "quit", Qt::QueuedConnection);
        }
    });
    QMetaObject::invokeMethod(timer, "start", Qt::QueuedConnection, Q_ARG(int, 0));

    if (wait && QThread::currentThread() != thread) {
        loop.exec();
    }
}

static QString g_pendingDeeplink;

QString Deeplink_ExtractFromArgs(const QStringList &args) {
    for (const auto &arg : args) {
        if (arg.startsWith("throne://")) return arg;
    }
    return {};
}

void Deeplink_Submit(const QString &url) {
    if (url.isEmpty() || !url.startsWith("throne://")) return;
    if (MW_handle_deeplink) {
        MW_handle_deeplink(url);
    } else {
        g_pendingDeeplink = url; // main window not up yet; replayed by Deeplink_FlushPending
    }
}

void Deeplink_FlushPending() {
    if (g_pendingDeeplink.isEmpty() || !MW_handle_deeplink) return;
    const QString url = g_pendingDeeplink;
    g_pendingDeeplink.clear();
    MW_handle_deeplink(url);
}

static QStringList g_pendingFiles;

QStringList LaunchFiles_ExtractFromArgs(const QStringList &args, const QDir &launchDir) {
    QStringList files;
    // Skip argv[0], our own flags, and the deeplink. "-appdata" is the only flag
    // taking a value, and that directory must not be read as a config to import.
    for (int i = 1; i < args.size(); i++) {
        const auto &arg = args[i];
        if (arg.startsWith('-')) {
            if (arg == "-appdata") i++;
            continue;
        }
        if (arg.startsWith("throne://")) continue;

        // Desktop launchers hand over file:// URLs, terminals plain paths, and a
        // relative path resolves against the directory we were launched from -
        // which main() abandons early on.
        auto path = arg.startsWith("file://") ? QUrl(arg).toLocalFile() : arg;
        if (path.isEmpty()) continue;
        const QFileInfo info(launchDir, path);
        if (info.isFile()) files << info.absoluteFilePath();
    }
    return files;
}

void LaunchFiles_Submit(const QStringList &paths) {
    if (paths.isEmpty()) return;
    if (MW_import_files) {
        MW_import_files(paths);
    } else {
        g_pendingFiles += paths; // main window not up yet; replayed by LaunchFiles_FlushPending
    }
}

void LaunchFiles_FlushPending() {
    if (g_pendingFiles.isEmpty() || !MW_import_files) return;
    const QStringList paths = g_pendingFiles;
    g_pendingFiles.clear();
    MW_import_files(paths);
}

void runOnNewThread(const std::function<void()> &callback, bool wait) {
    auto *timer = new QTimer();
    auto thread = new QThread();
    timer->moveToThread(thread);
    timer->setSingleShot(true);

    thread->start();
    QObject::connect(thread, &QThread::finished, thread, &QObject::deleteLater);

    QEventLoop loop;
    QObject::connect(timer, &QTimer::timeout, [=, &loop]() {
        callback();
        timer->deleteLater();
        QMetaObject::invokeMethod(thread, "quit", Qt::QueuedConnection);

        if (wait)
        {
            QMetaObject::invokeMethod(&loop, "quit", Qt::QueuedConnection);
        }
    });
    QMetaObject::invokeMethod(timer, "start", Qt::QueuedConnection, Q_ARG(int, 0));

    if (wait && QThread::currentThread() != thread) {
        loop.exec();
    }
}

void runOnThread(const std::function<void()> &callback, QObject *parent, bool wait) {
    auto *timer = new QTimer();
    auto thread = dynamic_cast<QThread *>(parent);
    if (thread == nullptr) {
        timer->moveToThread(parent->thread());
        thread = parent->thread();
    } else {
        timer->moveToThread(thread);
    }
    timer->setSingleShot(true);

    QEventLoop loop;
    QObject::connect(timer, &QTimer::timeout, [=, &loop]() {
        callback();
        timer->deleteLater();

        if (wait)
        {
            QMetaObject::invokeMethod(&loop, "quit", Qt::QueuedConnection);
        }
    });
    QMetaObject::invokeMethod(timer, "start", Qt::QueuedConnection, Q_ARG(int, 0));

    if (wait && QThread::currentThread() != thread) {
        loop.exec();
    }
}

void setTimeout(const std::function<void()> &callback, QObject *obj, int timeout) {
    auto t = new QTimer;
    QObject::connect(t, &QTimer::timeout, obj, [=] {
        callback();
        t->deleteLater();
    });
    t->setSingleShot(true);
    t->setInterval(timeout);
    t->start();
}
