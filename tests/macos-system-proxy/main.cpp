#include "include/sys/macos/ProxyJournal.hpp"
#include "include/sys/macos/ProxyServices.hpp"

#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLockFile>
#include <QThread>
#include <QTemporaryDir>
#include <functional>
#include <iostream>
#include <map>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>

using namespace MacSystemProxy;
#define CHECK(expression) do { if (!(expression)) throw std::runtime_error(#expression); } while (false)

struct MemoryJournal : Journal {
    QVector<Entry> entries;
    QString loadError, saveError;
    int saves = 0;
    QString load(QVector<Entry> &out) override { out = entries; return loadError; }
    QString save(const QVector<Entry> &in) override {
        ++saves;
        if (!saveError.isEmpty()) return saveError;
        entries = in;
        return {};
    }
};
struct Network : Backend {
    Journal &journal;
    std::map<QString, QJsonObject> live{{"wifi", {}}};
    QString listError, readError;
    int writes = 0, restores = 0;
    int failWrite = -1, failRestore = -1;
    bool failAfterCommit = false;
    std::function<void(const Entry &, bool)> beforeChange;
    explicit Network(Journal &journal) : journal(journal) {}
    QString services(QStringList &ids) override {
        ids.clear();
        for (const auto &[id, value] : live) ids << id;
        return listError;
    }
    QString read(const QString &id, QJsonObject &value, bool &exists) override {
        exists = live.count(id) != 0;
        if (exists) value = live.at(id);
        return readError;
    }
    Change change(const Entry &entry, bool restore, QString &error) override {
        QVector<Entry> recorded;
        CHECK(journal.load(recorded).isEmpty()); CHECK(!recorded.isEmpty()); // WAL precedes every network change.
        if (beforeChange) beforeChange(entry, restore);
        if (!live.count(entry.service)) return Change::Missing;
        auto &value = live.at(entry.service);
        const auto fields = keys(entry.protocol);
        const auto current = select(value, fields);
        const auto &expected = restore ? entry.applied : entry.before;
        const auto &desired = restore ? entry.before : entry.applied;
        if (current != expected && !(restore && current == desired)) return Change::Conflict;
        const int call = restore ? ++restores : ++writes;
        const bool failure = call == (restore ? failRestore : failWrite);
        if (failure && !failAfterCommit) { error = "simulated preference write error"; return Change::Failed; }
        for (const auto &key : fields) {
            if (desired.contains(key)) value.insert(key, desired.value(key));
            else value.remove(key);
        }
        if (failure) { error = "simulated activation error after commit"; return Change::Failed; }
        return Change::Done;
    }
};
struct Fixture {
    MemoryJournal journal;
    Network network{journal};
    Transaction transaction{network, journal};
};
static QJsonObject corporate() {
    return {{"HTTPEnable", 1}, {"HTTPProxy", "corp.example"}, {"HTTPPort", 3128}, {"HTTPProxyAuthenticated", 1},
            {"HTTPUser", "existing-keychain-user"}, {"ProxyAutoConfigEnable", 1},
            {"ProxyAutoConfigURLString", "https://corp.example/pac"}, {"ExceptionsList", QJsonArray{"*.corp"}}};
}
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    if (app.arguments().size() == 3 && app.arguments()[1] == "--hold-lock") {
        QLockFile lock(app.arguments()[2]); lock.setStaleLockTime(0);
        if (!lock.tryLock(0)) return 2;
        std::cout << "locked" << std::endl;
        QThread::sleep(30);
        return 0;
    }
    if (app.arguments().size() == 3 && app.arguments()[1] == "--crash-after-apply") {
        const auto path = app.arguments()[2]; QLockFile lock(path + ".lock"); lock.setStaleLockTime(0);
        if (!lock.tryLock(0)) return 2;
        FileJournal journal(path); Network network(journal); network.live["wifi"] = corporate();
        Transaction transaction(network, journal);
        if (!transaction.apply("127.0.0.1", 2080).isEmpty()) return 3;
        QFile state(path + ".network"); if (!state.open(QIODevice::WriteOnly)) return 4;
        state.write(QJsonDocument(network.live["wifi"]).toJson()); state.close();
        std::cout << "applied" << std::endl;
        QThread::sleep(30);
        return 0;
    }
    const std::map<const char *, std::function<void()>> tests{
        {"original proxy, auth flags, PAC and bypass values restored", [] {
            Fixture f; f.network.live["wifi"] = corporate();
            CHECK(f.transaction.apply("127.0.0.1", 2080).isEmpty());
            CHECK(f.network.live["wifi"].value("HTTPUser") == corporate().value("HTTPUser"));
            CHECK(f.transaction.recover().isEmpty());
            CHECK(f.network.live["wifi"] == corporate()); CHECK(f.journal.entries.isEmpty());
        }},
        {"Boolean flag types survive the roundtrip", [] {
            Fixture f; const QJsonObject original{{"HTTPEnable",false},{"HTTPProxyAuthenticated",true}};
            f.network.live["wifi"] = original; CHECK(f.transaction.apply("127.0.0.1", 2080).isEmpty());
            CHECK(f.transaction.recover().isEmpty()); CHECK(f.network.live["wifi"] == original);
        }},
        {"unsupported original key types fail before journaling or mutation", [] {
            Fixture f; f.network.live["wifi"].insert("HTTPPort", "not-a-number");
            CHECK(!f.transaction.apply("127.0.0.1", 2080).isEmpty()); CHECK(f.network.writes == 0); CHECK(f.journal.entries.isEmpty());
        }},
        {"absent keys restored as absent", [] {
            Fixture f; CHECK(f.transaction.apply("127.0.0.1", 2080).isEmpty());
            CHECK(f.transaction.recover().isEmpty()); CHECK(f.network.live["wifi"].isEmpty());
        }},
        {"networksetup enumeration failure leaves network untouched", [] {
            Fixture f; f.network.listError = "networksetup failed";
            CHECK(!f.transaction.apply("127.0.0.1", 2080).isEmpty()); CHECK(f.network.writes == 0); CHECK(f.journal.saves == 0);
        }},
        {"snapshot read failure leaves network untouched", [] {
            Fixture f; f.network.readError = "cannot read service";
            CHECK(!f.transaction.apply("127.0.0.1", 2080).isEmpty()); CHECK(f.network.writes == 0);
        }},
        {"journal write failure prevents network mutation", [] {
            Fixture f; f.journal.saveError = "disk full";
            CHECK(!f.transaction.apply("127.0.0.1", 2080).isEmpty()); CHECK(f.network.writes == 0);
        }},
        {"partial apply failure rolls back every changed protocol", [] {
            Fixture f; f.network.failWrite = 2;
            CHECK(!f.transaction.apply("127.0.0.1", 2080).isEmpty()); CHECK(f.network.live["wifi"].isEmpty()); CHECK(f.journal.entries.isEmpty());
        }},
        {"apply activation failure after commit rolls back", [] {
            Fixture f; f.network.failWrite = 2; f.network.failAfterCommit = true;
            CHECK(!f.transaction.apply("127.0.0.1", 2080).isEmpty()); CHECK(f.network.live["wifi"].isEmpty());
        }},
        {"failed rollback remains durable and retries", [] {
            Fixture f; f.network.failWrite = 2; f.network.failRestore = 1;
            CHECK(!f.transaction.apply("127.0.0.1", 2080).isEmpty()); CHECK(f.journal.entries.size() == 1);
            f.network.failRestore = -1; CHECK(f.transaction.recover().isEmpty()); CHECK(f.network.live["wifi"].isEmpty());
        }},
        {"restoration activation failure retries after commit", [] {
            Fixture f; CHECK(f.transaction.apply("127.0.0.1", 2080).isEmpty());
            f.network.failRestore = 1; f.network.failAfterCommit = true;
            CHECK(!f.transaction.recover().isEmpty()); CHECK(f.journal.entries.size() == 1);
            f.network.failRestore = -1; CHECK(f.transaction.recover().isEmpty()); CHECK(f.network.live["wifi"].isEmpty());
        }},
        {"repeated apply preserves the true original snapshot", [] {
            Fixture f; f.network.live["wifi"] = corporate();
            CHECK(f.transaction.apply("127.0.0.1", 2080).isEmpty()); CHECK(f.transaction.apply("::1", 2081).isEmpty());
            CHECK(f.transaction.recover().isEmpty()); CHECK(f.network.live["wifi"] == corporate());
        }},
        {"recreated transaction replays pending records", [] {
            Fixture f; CHECK(f.transaction.apply("127.0.0.1", 2080).isEmpty());
            Transaction restarted(f.network, f.journal);
            CHECK(restarted.recover().isEmpty()); CHECK(f.network.live["wifi"].isEmpty());
        }},
        {"third-party protocol edit wins without affecting other protocols", [] {
            Fixture f; CHECK(f.transaction.apply("127.0.0.1", 2080).isEmpty());
            f.network.live["wifi"].insert("HTTPPort", 9999);
            CHECK(f.transaction.recover().isEmpty()); CHECK(f.network.live["wifi"].value("HTTPPort").toInt() == 9999);
            CHECK(!f.network.live["wifi"].contains("SOCKSProxy"));
        }},
        {"third-party PAC edits survive restoration", [] {
            Fixture f; CHECK(f.transaction.apply("127.0.0.1", 2080).isEmpty());
            f.network.live["wifi"].insert("ProxyAutoConfigURLString", "https://new.example/pac");
            CHECK(f.transaction.recover().isEmpty());
            CHECK(f.network.live["wifi"] == QJsonObject({{"ProxyAutoConfigURLString", "https://new.example/pac"}}));
        }},
        {"conflict between snapshot and apply rolls back earlier writes", [] {
            Fixture f;
            f.network.beforeChange = [&](const Entry &e, bool restore) {
                if (!restore && e.protocol == 1) f.network.live["wifi"].insert("HTTPSProxy", "another-app");
            };
            CHECK(!f.transaction.apply("127.0.0.1", 2080).isEmpty());
            CHECK(f.network.live["wifi"] == QJsonObject({{"HTTPSProxy", "another-app"}}));
        }},
        {"deleted service not recreated, newly added service not cleared", [] {
            Fixture f; CHECK(f.transaction.apply("127.0.0.1", 2080).isEmpty());
            f.network.live.erase("wifi"); f.network.live["new-wifi"] = corporate();
            CHECK(f.transaction.recover().isEmpty()); CHECK(f.network.live.size() == 1); CHECK(f.network.live["new-wifi"] == corporate());
        }},
        {"multiple services restored by identity", [] {
            Fixture f; f.network.live["ethernet"] = corporate();
            CHECK(f.transaction.apply("127.0.0.1", 2080).isEmpty()); CHECK(f.journal.entries.size() == 6);
            CHECK(f.transaction.recover().isEmpty()); CHECK(f.network.live["ethernet"] == corporate()); CHECK(f.network.live["wifi"].isEmpty());
        }},
        {"clear without journal never touches another application proxy", [] {
            Fixture f; f.network.live["wifi"] = corporate(); CHECK(f.transaction.recover().isEmpty());
            CHECK(f.network.restores == 0); CHECK(f.network.live["wifi"] == corporate());
        }},
        {"invalid endpoint leaves outstanding recovery intact", [] {
            Fixture f; CHECK(f.transaction.apply("127.0.0.1", 2080).isEmpty());
            CHECK(!f.transaction.apply("", -1).isEmpty()); CHECK(f.journal.entries.size() == 3);
        }},
        {"corrupt journal is preserved and prevents apply", [] {
            Fixture f; f.journal.loadError = "corrupt state";
            CHECK(!f.transaction.apply("127.0.0.1", 2080).isEmpty()); CHECK(f.network.writes == 0); CHECK(f.journal.saves == 0);
        }},
        {"journal cleanup failure retries safely", [] {
            Fixture f; CHECK(f.transaction.apply("127.0.0.1", 2080).isEmpty());
            f.journal.saveError = "read-only disk"; CHECK(!f.transaction.recover().isEmpty()); CHECK(f.journal.entries.size() == 3);
            f.journal.saveError.clear(); CHECK(f.transaction.recover().isEmpty()); CHECK(f.journal.entries.isEmpty());
        }},
        {"networksetup nonzero exit and crash reject even valid stdout", [] {
            QTemporaryDir dir;
            const auto path = dir.filePath("networksetup");
            for (const auto &ending : {QByteArray("exit 7"), QByteArray("kill -KILL $$")}) {
                QFile file(path); CHECK(file.open(QIODevice::WriteOnly));
                file.write("#!/bin/sh\nprintf 'legend\\nWi-Fi\\n'\n" + ending + "\n");
                CHECK(file.setPermissions(QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner)); file.close();
                QStringList names; CHECK(!discoverServices(path, names, 1000).isEmpty()); CHECK(names.isEmpty());
            }
        }},
        {"networksetup timeout kills and reaps the command", [] {
            QTemporaryDir dir; const auto path = dir.filePath("networksetup");
            QFile file(path); CHECK(file.open(QIODevice::WriteOnly)); file.write("#!/bin/sh\nexec /bin/sleep 5\n");
            CHECK(file.setPermissions(QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner)); file.close();
            QStringList names; CHECK(!discoverServices(path, names, 50).isEmpty()); CHECK(names.isEmpty());
        }},
        {"networksetup disabled and whitespace service names parsed", [] {
            QStringList names;
            CHECK(!enabledServiceNames(true, {}, names).isEmpty());
            CHECK(enabledServiceNames(true, "legend\nWi-Fi\n*disabled\nUSB LAN\n", names).isEmpty());
            CHECK(names == QStringList({"Wi-Fi", "USB LAN"}));
        }},
        {"SIGKILL leaves a disk journal recoverable by a new transaction", [] {
            QTemporaryDir dir; const auto path = dir.filePath("state.json");
            QProcess owner; owner.start(QCoreApplication::applicationFilePath(), {"--crash-after-apply", path});
            CHECK(owner.waitForReadyRead(5000)); CHECK(owner.readAllStandardOutput().contains("applied"));
            owner.kill(); CHECK(owner.waitForFinished(5000));
            QLockFile nextOwner(path + ".lock"); nextOwner.setStaleLockTime(0); CHECK(nextOwner.tryLock(0));
            FileJournal journal(path); Network restarted(journal);
            QFile state(path + ".network"); CHECK(state.open(QIODevice::ReadOnly));
            restarted.live["wifi"] = QJsonDocument::fromJson(state.readAll()).object();
            CHECK(restarted.live["wifi"] != corporate());
            Transaction transaction(restarted, journal); CHECK(transaction.recover().isEmpty());
            CHECK(restarted.live["wifi"] == corporate()); CHECK(!QFile::exists(path));
        }},
        {"live owner is excluded; killed owner lock is reclaimed", [] {
            QTemporaryDir dir; const auto path = dir.filePath("owner.lock");
            QProcess owner; owner.start(QCoreApplication::applicationFilePath(), {"--hold-lock", path});
            CHECK(owner.waitForReadyRead(5000)); CHECK(owner.readAllStandardOutput().contains("locked"));
            QLockFile contender(path); contender.setStaleLockTime(0);
            CHECK(!contender.tryLock(0)); owner.kill(); CHECK(owner.waitForFinished(5000));
            CHECK(contender.tryLock(0));
        }},
        {"credentials and unrelated proxy keys are never journaled", [] {
            Fixture f; f.network.live["wifi"] = corporate();
            CHECK(f.transaction.apply("127.0.0.1", 2080).isEmpty());
            for (const auto &entry : f.journal.entries) {
                CHECK(!entry.before.contains("HTTPUser")); CHECK(!entry.before.contains("ProxyAutoConfigURLString"));
                CHECK(!entry.applied.contains("HTTPUser"));
            }
        }},
        {"file journal rejects duplicate and extra-key records", [] {
            QTemporaryDir dir; const auto path = dir.filePath("state.json"); FileJournal journal(path);
            Entry entry{"wifi", 0, {}, {{"HTTPEnable",1},{"HTTPProxy","127.0.0.1"},{"HTTPPort",2080},{"HTTPProxyAuthenticated",0}}};
            CHECK(journal.save({entry, entry}).isEmpty()); QVector<Entry> loaded;
            CHECK(!journal.load(loaded).isEmpty()); CHECK(QFile::remove(path));
            entry.before.insert("ProxyAutoConfigURLString", "must-not-restore");
            CHECK(journal.save({entry}).isEmpty()); CHECK(!journal.load(loaded).isEmpty());
        }},
        {"file journal roundtrip uses 0600 and preserves absent keys", [] {
            QTemporaryDir dir; CHECK(dir.isValid()); FileJournal journal(dir.filePath("state.json"));
            const QVector<Entry> entries{{"wifi", 0, {}, {{"HTTPEnable",1},{"HTTPProxy","::1"},{"HTTPPort",2080},{"HTTPProxyAuthenticated",0}}}};
            CHECK(journal.save(entries).isEmpty()); struct stat st{};
            CHECK(::stat(QFile::encodeName(dir.filePath("state.json")).constData(), &st) == 0); CHECK((st.st_mode & 0777) == 0600);
            QVector<Entry> loaded; CHECK(journal.load(loaded).isEmpty()); CHECK(loaded.size() == 1); CHECK(loaded[0].before.isEmpty());
            CHECK(journal.save({}).isEmpty()); CHECK(!QFile::exists(dir.filePath("state.json")));
        }},
        {"file journal rejects malformed and unsupported versions", [] {
            QTemporaryDir dir; const auto path = dir.filePath("state.json"); FileJournal journal(path);
            for (const auto &data : {QByteArray("{"), QByteArray("{\"version\":2,\"entries\":[]}"), QByteArray("{\"version\":1,\"entries\":[{}]}")}) {
                QFile file(path); CHECK(file.open(QIODevice::WriteOnly)); CHECK(file.setPermissions(QFile::ReadOwner|QFile::WriteOwner)); file.write(data); file.close();
                QVector<Entry> loaded; CHECK(!journal.load(loaded).isEmpty()); CHECK(QFile::exists(path));
            }
        }},
        {"file journal rejects symlinks and broad permissions", [] {
            QTemporaryDir dir; const auto target = dir.filePath("target"); QFile file(target); CHECK(file.open(QIODevice::WriteOnly)); file.write("sensitive"); file.close();
            const auto path = dir.filePath("state.json"); CHECK(QFile::link(target, path)); FileJournal journal(path); QVector<Entry> loaded;
            CHECK(!journal.load(loaded).isEmpty()); CHECK(!journal.save({}).isEmpty()); CHECK(QFile::remove(path));
            CHECK(file.setPermissions(QFile::ReadOwner|QFile::WriteOwner|QFile::ReadOther)); CHECK(QFile::rename(target,path)); CHECK(!journal.load(loaded).isEmpty());
        }}
    };
    int failed = 0;
    for (const auto &[name, test] : tests) {
        try { test(); std::cout << "PASS " << name << '\n'; }
        catch (const std::exception &error) { ++failed; std::cerr << "FAIL " << name << ": " << error.what() << '\n'; }
    }
    std::cout << tests.size() - failed << '/' << tests.size() << " passed\n";
    return failed ? 1 : 0;
}
