#include "include/global/Utils.hpp"

#include "3rdparty/QThreadCreateThread.hpp"

#include <random>

#include <QApplication>
#include <QUrlQuery>
#include <QTcpServer>
#include <QTimer>
#include <QMessageBox>
#include <QFile>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QDateTime>
#include <QLocale>

#ifdef Q_OS_WIN
#include "include/sys/windows/guihelper.h"
#endif
#ifdef Q_OS_MAC
#include <ApplicationServices/ApplicationServices.h>
#endif

QStringList SplitLines(const QString &_string) {
    return _string.split(QRegularExpression("[\r\n]"), Qt::SplitBehaviorFlags::SkipEmptyParts);
}

QStringList SplitLinesSkipSharp(const QString &_string, int maxLine) {
    auto lines = SplitLines(_string);
    QStringList newLines;
    int i = 0;
    for (const auto &line: lines) {
        if (line.trimmed().startsWith("#")) continue;
        newLines << line;
        if (maxLine > 0 && ++i >= maxLine) break;
    }
    return newLines;
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

QString GenRandomLoopback() {
#ifdef Q_OS_MACOS
    return "127.0.0.1";
#else
    std::random_device rd;
    std::mt19937 mt(rd());
    std::uniform_int_distribution<int> octet(1, 254);
    return QString("127.%1.%2.%3").arg(octet(mt)).arg(octet(mt)).arg(octet(mt));
#endif
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
    bool isEmpty = true;
    for (auto &item: list) {
        if (item.trimmed().isEmpty()) continue;
        list2.append(item);
        isEmpty = false;
    }

    if (isEmpty) return {};
    else return QJsonArray::fromVariantList(list2);
}

QJsonArray QListInt2QJsonArray(const QList<int> &list) {
    QVariantList list2;
    for (auto &item: list)
        list2.append(item);
    return QJsonArray::fromVariantList(list2);
}

QList<int> QJsonArray2QListInt(const QJsonArray &arr) {
    QList<int> list2;
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
    for (const auto &key: mp.keys()) {
        res.insert(key, mp[key]);
    }

    return res;
}

QList<QString> QListInt2QListString(const QList<int> &list) {
    QList<QString> resp;
    for (int item : list) resp << Int2String(item);
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

int MkPort() {
    QTcpServer s;
    s.listen();
    auto port = s.serverPort();
    s.close();
    return port;
}

QList<int> MkManyPorts(int num) {
    QList<int> res;
    QList<QTcpServer*> servers;
    for (int i=0;i<num;i++) {
        auto server = new QTcpServer();
        server->listen();
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
    auto address = QHostAddress(str);
    if (address.protocol() == QAbstractSocket::IPv4Protocol)
        return true;
    return false;
}

bool IsIpAddressV6(const QString &str) {
    auto address = QHostAddress(str);
    if (address.protocol() == QAbstractSocket::IPv6Protocol)
        return true;
    return false;
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
    // any thread
    auto thread = mainwindow->thread();
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
