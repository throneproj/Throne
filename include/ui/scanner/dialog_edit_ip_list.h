#pragma once

#include <QDialog>
#include <QList>
#include <memory>

#include "include/database/entities/IpList.h"
#include "ui_dialog_edit_ip_list.h"

QT_BEGIN_NAMESPACE
namespace Ui {
    class DialogEditIpList;
}
QT_END_NAMESPACE

class QTimer;

namespace Scanner {
    struct ImportOutcome;
}

namespace Validation {
    class Form;
}

class DialogEditIpList : public QDialog {
    Q_OBJECT

public:
    explicit DialogEditIpList(QWidget *parent, int listId,
                              Configs::IpList::SourceKind kind = Configs::IpList::SourceKind::Manual,
                              const QString &prefillText = {});

    ~DialogEditIpList() override;

    [[nodiscard]] int ListId() const { return savedId; }

public slots:
    void accept() override;

    void reject() override;

private:
    Ui::DialogEditIpList *ui;

    std::shared_ptr<Configs::IpList> list;

    bool isNew = true;

    bool scanOwned = false;

    bool missing = false;

    int savedId = -1;

    QList<Configs::IpListEntry> fetched;

    bool hasFetch = false;

    Configs::IpList::SourceKind fetchedKind = Configs::IpList::SourceKind::Manual;

    QString fetchedSource;

    int fetchedRejected = 0;

    QStringList fetchedSamples;

    int editorTotal = 0;

    bool editorFromDb = false;

    bool editorTruncated = false;

    bool fetching = false;

    bool saving = false;

    bool acceptAfterFetch = false;

    bool summaryIsError = false;

    QTimer *summaryTimer = nullptr;

    Validation::Form *form = nullptr;

    [[nodiscard]] Configs::IpList::SourceKind currentKind() const;

    [[nodiscard]] QString currentSource() const;

    [[nodiscard]] QString sourceError(Configs::IpList::SourceKind kind, const QString &source) const;

    [[nodiscard]] QString defaultName() const;

    [[nodiscard]] bool fetchMatchesInputs() const;

    void loadList(int listId);

    void setupRuleSetPicker();

    void showEntries(const QList<Configs::IpListEntry> &entries, int total, bool fromDb);

    void updateControls();

    void updateSummary();

    void setSummary(const QString &text, bool isError);

    void applyColors();

    void startFetch(bool thenAccept);

    void finishSave(const std::shared_ptr<Configs::IpList> &candidate, bool stored);

    void onFetched(Configs::IpList::SourceKind kind, const QString &source, const Scanner::ImportOutcome &outcome);
};
