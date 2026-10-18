#include "include/global/Utils.hpp"
#include "include/global/Logger.hpp"

#include "3rdparty/QThreadCreateThread.hpp"

#include <atomic>
#include <mutex>
#include <random>
#include <utility>

#include <QApplication>
#include <QUrlQuery>
#include <QTcpServer>
#include <QTimer>
#include <QMessageBox>
#include <QPointer>
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
#include <QFontDatabase>
#include <QFontInfo>
#include <QTextBoundaryFinder>

#ifdef Q_OS_WIN
#include "include/sys/windows/guihelper.h"
#endif
#ifdef Q_OS_MAC
#include <ApplicationServices/ApplicationServices.h>
#endif

namespace {
    using UserLogSink = std::function<void(const QString &)>;
    using DialogMessageSink = std::function<void(MwMessage, const QStringList &)>;

    std::mutex g_sinkMu;
    std::shared_ptr<const UserLogSink> g_userLogSink;
    std::shared_ptr<const DialogMessageSink> g_dialogMessageSink;

    std::atomic<int> g_headlessDialogs{0};

    template<typename Sink>
    std::shared_ptr<const Sink> loadSink(const std::shared_ptr<const Sink> &slot) {
        std::lock_guard lock(g_sinkMu);
        return slot;
    }

    template<typename Sink>
    void storeSink(std::shared_ptr<const Sink> &slot, Sink sink) {
        auto next = sink ? std::make_shared<const Sink>(std::move(sink)) : nullptr;
        std::shared_ptr<const Sink> previous; // outlives the lock, so the old sink is never destroyed under it
        std::lock_guard lock(g_sinkMu);
        previous = std::exchange(slot, std::move(next));
    }
}

void MW_show_log(const QString &line) {
    try {
        if (const auto sink = loadSink(g_userLogSink)) {
            (*sink)(line);
        } else {
            Logging::WriteUserLog(line);
        }
    } catch (...) {
    }
}

void SetUserLogSink(std::function<void(const QString &)> sink) {
    storeSink(g_userLogSink, std::move(sink));
}

void MW_dialog_message(MwMessage cmd, const QStringList &args) {
    const auto sink = loadSink(g_dialogMessageSink);
    if (!sink) {
        LOG_DEBUG(QString("dialog message %1 dropped: no sink installed").arg(static_cast<int>(cmd)));
        return;
    }
    (*sink)(cmd, args);
}

void SetDialogMessageSink(std::function<void(MwMessage, const QStringList &)> sink) {
    storeSink(g_dialogMessageSink, std::move(sink));
}

bool IsGuiApp() {
    const auto *app = QCoreApplication::instance();
    return app != nullptr && app->inherits("QApplication");
}

int HeadlessDialogCount() {
    return g_headlessDialogs.load(std::memory_order_relaxed);
}

void LogHeadlessDialog(const QString &kind, const QString &title, const QString &text, const QString &defaultValue) {
    g_headlessDialogs.fetch_add(1, std::memory_order_relaxed);
    QString flat = text;
    flat.replace(QStringLiteral("\r\n"), QStringLiteral(" | ")).replace('\n', QStringLiteral(" | ")).replace('\r', QStringLiteral(" | "));
    if (flat.size() > 2000) flat.truncate(2000);
    LOG_WARN(QString("HEADLESS-DIALOG kind=%1 title=\"%2\" text=\"%3\" default=%4").arg(kind, title, flat, defaultValue));
}

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

QJsonObject QString2QJsonObject(const QString &jsonString) {
    QJsonDocument jsonDocument = QJsonDocument::fromJson(jsonString.toUtf8());
    QJsonObject jsonObject = jsonDocument.object();
    return jsonObject;
}

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

QString ShortDisplayName(const QString &name, int maxGraphemes) {
    if (name.size() <= maxGraphemes) return name;
    QTextBoundaryFinder finder(QTextBoundaryFinder::Grapheme, name);
    qsizetype cut = 0;
    for (int i = 0; i < maxGraphemes - 1; ++i) {
        cut = finder.toNextBoundary();
        if (cut < 0) return name;
    }
    const qsizetype end = finder.toNextBoundary();
    if (end < 0 || end == name.size()) return name;
    return name.left(cut).trimmed() + QStringLiteral("…");
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
    if (!IsGuiApp()) {
        LogHeadlessDialog(QStringLiteral("GetMessageBoxParent"), {}, {}, QStringLiteral("nullptr"));
        return nullptr;
    }
    auto parent = QApplication::activeWindow();
    // A child box dies with its parent box, even while it is still running on the caller's stack.
    while (qobject_cast<QMessageBox *>(parent) != nullptr) {
        parent = parent->parentWidget() != nullptr ? parent->parentWidget()->window() : nullptr;
    }
    if (parent == nullptr && mainwindow != nullptr && mainwindow->isVisible()) return mainwindow;
    return parent;
}

int MessageBoxWarning(const QString &title, const QString &text) {
    if (!IsGuiApp()) {
        LogHeadlessDialog(QStringLiteral("MessageBoxWarning"), title, text, QStringLiteral("Ok"));
        return QMessageBox::Ok;
    }
    return QMessageBox::warning(GetMessageBoxParent(), title, text);
}

void ShowPassiveWarning(const QString &title, const QString &text) {
    if (!IsGuiApp()) {
        LogHeadlessDialog(QStringLiteral("ShowPassiveWarning"), title, text, QStringLiteral("none"));
        return;
    }
    static QPointer<QMessageBox> box;
    if (box) {
        box->setWindowTitle(title);
        box->setText(text);
        box->raise();
        return;
    }
    box = new QMessageBox(QMessageBox::Warning, title, text, QMessageBox::Ok, GetMessageBoxParent());
    // Callers embed foreign text (adapter names, SQLite messages); AutoText would render a tag-like one as HTML.
    box->setTextFormat(Qt::PlainText);
    box->setAttribute(Qt::WA_DeleteOnClose);
    box->setWindowModality(Qt::NonModal);
    box->show();
}

void PostPassiveWarning(const QString &title, const QString &text) {
    if (!IsGuiApp()) {
        LogHeadlessDialog(QStringLiteral("PostPassiveWarning"), title, text, QStringLiteral("none"));
        return;
    }
    auto *app = QCoreApplication::instance();
    if (app == nullptr) return;
    QMetaObject::invokeMethod(app, [title, text] { ShowPassiveWarning(title, text); }, Qt::QueuedConnection);
}

int MessageBoxInfo(const QString &title, const QString &text) {
    if (!IsGuiApp()) {
        LogHeadlessDialog(QStringLiteral("MessageBoxInfo"), title, text, QStringLiteral("Ok"));
        return QMessageBox::Ok;
    }
    return QMessageBox::information(GetMessageBoxParent(), title, text);
}

void MessageBoxScrollable(const QString &title, const QString &text) {
    if (!IsGuiApp()) {
        LogHeadlessDialog(QStringLiteral("MessageBoxScrollable"), title, text, QStringLiteral("none"));
        return;
    }
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
    if (!IsGuiApp()) {
        LogHeadlessDialog(QStringLiteral("MessageBoxCheck"), title, text, QStringLiteral("Cancel"));
        return QMessageBox::Cancel;
    }
    QMessageBox msgBox(GetMessageBoxParent());
    msgBox.setWindowTitle(title);
    msgBox.setTextFormat(Qt::PlainText);
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
    if (!IsGuiApp()) return;
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
    if (!IsGuiApp()) return;
    w->hide();
#ifdef Q_OS_MAC
    ProcessSerialNumber psn = { 0, kCurrentProcess };
    TransformProcessType(&psn, kProcessTransformToUIElementApplication);
#endif
}

QStringList LogFontFamilies(const QString &preferred) {
    QStringList families;
    if (const QString chosen = preferred.trimmed(); !chosen.isEmpty()) families << chosen;
    families << QStringList{"Consolas", "Menlo", "SF Mono", "DejaVu Sans Mono", "Noto Sans Mono", "Ubuntu Mono", "monospace"};
    families.removeDuplicates();
    return families;
}

QString ResolveLogFontFamily(const QString &preferred) {
    if (!IsGuiApp()) {
        const QString family = LogFontFamilies(preferred).first();
        LogHeadlessDialog(QStringLiteral("ResolveLogFontFamily"), {}, {}, family);
        return family;
    }
    for (const auto &family : LogFontFamilies(preferred)) {
        if (family != QStringLiteral("monospace") && QFontDatabase::hasFamily(family)) return family;
    }
    QFont generic(QStringLiteral("monospace"));
    generic.setStyleHint(QFont::Monospace);
    return QFontInfo(generic).family();
}

void runOnUiThread(const std::function<void()> &callback, bool wait) {
    auto *app = QCoreApplication::instance();
    if (app == nullptr) return;
    auto thread = app->thread();
    if (thread == QThread::currentThread()) {
        callback();
        return;
    }
    if (!wait) {
        QMetaObject::invokeMethod(app, callback, Qt::QueuedConnection);
        return;
    }
    auto *timer = new QTimer();
    timer->moveToThread(thread);
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

// The next argument after one of these (one or two dashes) is its value, never a link or a config to import; "--flag=value"
// carries its own.
static bool launchArgTakesValue(const QString &arg) {
    static const QStringList names = {
        QStringLiteral("appdata"), QStringLiteral("dump-configs"), QStringLiteral("data-dir"), QStringLiteral("api-listen"),
        QStringLiteral("api-key-file"), QStringLiteral("api-allow"), QStringLiteral("restart-mode"), QStringLiteral("log-format"),
    };
    const int dashes = arg.startsWith(QLatin1String("--")) ? 2 : (arg.startsWith('-') ? 1 : 0);
    return dashes > 0 && names.contains(arg.mid(dashes));
}

static QString g_pendingDeeplink;

QString Deeplink_ExtractFromArgs(const QStringList &args) {
    for (int i = 0; i < args.size(); i++) {
        if (launchArgTakesValue(args[i])) {
            i++;
            continue;
        }
        if (args[i].startsWith("throne://")) return args[i];
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

QString Deeplink_TakePending() {
    return std::exchange(g_pendingDeeplink, {});
}

static QStringList g_pendingFiles;

QStringList LaunchFiles_ExtractFromArgs(const QStringList &args, const QDir &launchDir) {
    QStringList files;
    for (int i = 1; i < args.size(); i++) {
        const auto &arg = args[i];
        if (arg.startsWith('-')) {
            if (launchArgTakesValue(arg)) i++;
            continue;
        }
        if (arg.startsWith("throne://")) continue;

        // A relative path resolves against the directory we were launched from, which main() abandons early.
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

QStringList LaunchFiles_TakePending() {
    return std::exchange(g_pendingFiles, {});
}

void runOnNewThread(const std::function<void()> &callback, bool wait) {
    auto *timer = new QTimer();
    auto thread = new QThread();

    if (auto *app = QCoreApplication::instance()) thread->moveToThread(app->thread());
    timer->moveToThread(thread);
    timer->setSingleShot(true);

    thread->start();
    QObject::connect(thread, &QThread::finished, thread, &QObject::deleteLater);

    QEventLoop loop;
    QObject::connect(timer, &QTimer::timeout, [=, &loop]() {
        callback();
        timer->deleteLater();
        thread->quit();

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
