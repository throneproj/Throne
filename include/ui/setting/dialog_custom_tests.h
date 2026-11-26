#pragma once

#include <QDialog>
#include <QFont>
#include <QList>

#include "include/database/entities/CustomTest.h"

class JsCodeEdit;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTextBrowser;
class ReorderListWidget;

class DialogCustomTests : public QDialog {
    Q_OBJECT

public:
    explicit DialogCustomTests(const QList<Configs::CustomTest> &initialTests, const QFont &editorFont,
                               QWidget *parent = nullptr);

    [[nodiscard]] const QList<Configs::CustomTest> &resultTests() const { return tests_; }

public slots:
    void accept() override;

private:
    void rebuildList(int selectRow);
    void loadRow(int row);
    void syncEditorState();
    void updateButtonStates();
    void addTest();
    void duplicateCurrentTest();
    void removeCurrentTest();
    void checkSyntax();
    QString defaultTestName() const;

    QList<Configs::CustomTest> tests_;
    int currentRow_ = -1;
    bool updatingUi_ = false;

    ReorderListWidget *list_ = nullptr;
    QPushButton *addBtn_ = nullptr;
    QPushButton *dupBtn_ = nullptr;
    QPushButton *removeBtn_ = nullptr;
    QWidget *editorPane_ = nullptr;
    QLineEdit *nameEdit_ = nullptr;
    QSpinBox *timeoutSpin_ = nullptr;
    JsCodeEdit *scriptEdit_ = nullptr;
    QPushButton *helpToggleBtn_ = nullptr;
    QTextBrowser *helpBox_ = nullptr;
    QPushButton *checkBtn_ = nullptr;
    QLabel *statusLabel_ = nullptr;
    QFont editorFont_;
};
