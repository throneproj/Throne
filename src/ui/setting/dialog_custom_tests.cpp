#include "include/ui/setting/dialog_custom_tests.h"

#include <QApplication>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QSplitter>
#include <QTextBrowser>
#include <QVBoxLayout>

#include "include/api/RPC.h"
#include "include/engine/services/SettingsService.h"
#include "include/ui/setting/ThemeManager.hpp"
#include "include/ui/utils/GuiUtils.hpp"
#include "include/ui/utils/ReorderListWidget.h"
#include "include/ui/widget/JsCodeEdit.h"

namespace {
    QString customTestsApiHelpHtml(const QFont &monoFont, const QPalette &palette) {
        const auto &tokens = themeManager()->tokens;
        const QColor base = palette.color(QPalette::Base);
        const QColor text = palette.color(QPalette::Text);
        const QColor chip = QColor::fromRgbF(
            base.redF() * 0.92 + text.redF() * 0.08,
            base.greenF() * 0.92 + text.greenF() * 0.08,
            base.blueF() * 0.92 + text.blueF() * 0.08);
        const QString codeBg = chip.name(QColor::HexRgb);
        const QString muted = tokens.muted.isValid() ? tokens.muted.name() : QStringLiteral("#6c757d");
        const QString monoFamily = monoFont.family().toHtmlEscaped();

        const auto code = [&codeBg, &monoFamily](const QString &text) {
            return QStringLiteral("<code style=\"font-family:'%1',monospace;background-color:%2;\">%3</code>")
                .arg(monoFamily, codeBg, text.toHtmlEscaped());
        };

        return QStringLiteral(
            "<style>"
            "body { margin: 4px 6px; line-height: 1.45; }"
            "h4 { margin: 8px 0 3px 0; font-weight: 600; }"
            "h4:first-child { margin-top: 2px; }"
            "ul { margin: 2px 0 4px 0; padding-left: 18px; }"
            "li { margin: 2px 0; }"
            "p { margin: 2px 0 4px 0; }"
            "</style>"
            "<h4>%1</h4>"
            "<p>%2</p>"
            "<h4>%3</h4>"
            "<ul>"
            "<li>%4</li>"
            "<li>%5<ul>"
            "<li>%6</li>"
            "<li>%7</li>"
            "<li>%8</li>"
            "<li>%9</li>"
            "</ul></li>"
            "</ul>"
            "<h4>%10 <span style=\"font-weight:normal;color:%11;\">(%12)</span></h4>"
            "<ul>"
            "<li>%13</li>"
            "<li>%14</li>"
            "<li>%15</li>"
            "<li>%16</li>"
            "</ul>")
            .arg(DialogCustomTests::tr("Entry Point"),
                 DialogCustomTests::tr("Scripts may define multiple helper functions; %1 (sync or %2) is the entry point and returns the result. %3 holds the configured timeout in milliseconds.")
                     .arg(code(QStringLiteral("test(ctx)")),
                          code(QStringLiteral("async function test(ctx)")),
                          code(QStringLiteral("ctx.timeoutMs"))),
                 DialogCustomTests::tr("Return Value"),
                 DialogCustomTests::tr("Primitive (%1): displayed directly (numbers sort numerically; text sorts alphabetically).")
                     .arg(code(QStringLiteral("string | number | boolean"))),
                 DialogCustomTests::tr("Object %1:")
                     .arg(code(QStringLiteral("{ text, color?, sortKey?, tooltip? }"))),
                 DialogCustomTests::tr("%1 — text displayed in the table column.")
                     .arg(code(QStringLiteral("text"))),
                 DialogCustomTests::tr("%1 (optional) — semantic name (%2) or CSS hex color (%3).")
                     .arg(code(QStringLiteral("color")),
                          code(QStringLiteral("\"green\", \"yellow\", \"red\", \"blue\", \"gray\"")),
                          code(QStringLiteral("\"#2b8a3e\""))),
                 DialogCustomTests::tr("%1 (optional) — number or string used for sorting; when omitted, sorting uses the display text.")
                     .arg(code(QStringLiteral("sortKey"))),
                 DialogCustomTests::tr("%1 (optional) — hover tooltip text.")
                     .arg(code(QStringLiteral("tooltip"))),
                 DialogCustomTests::tr("Built-in Helpers"),
                 muted,
                 DialogCustomTests::tr("all network traffic goes through the tested proxy"),
                 DialogCustomTests::tr("%1 — performs an HTTP request and returns %2. Options: %3.")
                     .arg(code(QStringLiteral("fetch(url, opts?)")),
                          code(QStringLiteral("{ status, statusText, ok, url, headers, elapsedMs, truncated, text(), json() }")),
                          code(QStringLiteral("method, headers, body, timeoutMs, followRedirects, maxRedirects, maxBytes, insecure"))),
                 DialogCustomTests::tr("%1 — convenience HTTP helpers returning %2.")
                     .arg(code(QStringLiteral("http.get / head / post / request")),
                          code(QStringLiteral("{ status, statusText, ok, url, headers, text, body, elapsedMs, truncated, json() }"))),
                 DialogCustomTests::tr("%1 — measures TCP connection latency and returns %2.")
                     .arg(code(QStringLiteral("tcpPing(host, port, timeoutMs?)")),
                          code(QStringLiteral("{ ok, latencyMs, error }"))),
                 DialogCustomTests::tr("%1 — wait, Base64 encode/decode, and log output.")
                     .arg(code(QStringLiteral("sleep(ms), btoa(str), atob(str), console.log(...)"))));
    }
} // namespace

DialogCustomTests::DialogCustomTests(const QList<Configs::CustomTest> &initialTests, const QFont &editorFont,
                                     QWidget *parent)
    : QDialog(parent), tests_(initialTests), editorFont_(editorFont) {
    setWindowTitle(tr("Custom Tests"));
    resize(860, 580);

    auto *rootLayout = new QVBoxLayout(this);
    auto *splitter = new QSplitter(Qt::Horizontal, this);
    rootLayout->addWidget(splitter, 1);

    // Left pane: list + Add / Duplicate / Delete buttons.
    auto *leftWidget = new QWidget(splitter);
    auto *leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->setContentsMargins(0, 0, 0, 0);

    list_ = new ReorderListWidget(leftWidget);
    leftLayout->addWidget(list_, 1);

    auto *listBtnLayout = new QHBoxLayout();
    addBtn_ = new QPushButton(tr("Add"), leftWidget);
    dupBtn_ = new QPushButton(tr("Duplicate"), leftWidget);
    removeBtn_ = new QPushButton(tr("Delete"), leftWidget);
    listBtnLayout->addWidget(addBtn_);
    listBtnLayout->addWidget(dupBtn_);
    listBtnLayout->addWidget(removeBtn_);
    leftLayout->addLayout(listBtnLayout);

    // Right pane: metadata + script editor + API help + Check Syntax.
    editorPane_ = new QWidget(splitter);
    auto *rightLayout = new QVBoxLayout(editorPane_);
    rightLayout->setContentsMargins(0, 0, 0, 0);

    auto *formLayout = new QFormLayout();
    auto *metaRow = new QHBoxLayout();
    nameEdit_ = new QLineEdit(editorPane_);
    nameEdit_->setPlaceholderText(tr("Test name"));
    timeoutSpin_ = new QSpinBox(editorPane_);
    timeoutSpin_->setRange(500, 60000);
    timeoutSpin_->setSingleStep(500);
    timeoutSpin_->setSuffix(QStringLiteral(" ms"));
    metaRow->addWidget(nameEdit_, 1);
    metaRow->addWidget(new QLabel(tr("Timeout (ms)"), editorPane_));
    metaRow->addWidget(timeoutSpin_);
    formLayout->addRow(tr("Name"), metaRow);
    rightLayout->addLayout(formLayout);

    auto *toolbarLayout = new QHBoxLayout();
    auto *scriptLabel = new QLabel(tr("Script (JavaScript)"), editorPane_);
    helpToggleBtn_ = new QPushButton(tr("API Reference"), editorPane_);
    helpToggleBtn_->setCheckable(true);
    toolbarLayout->addWidget(scriptLabel);
    toolbarLayout->addStretch();
    toolbarLayout->addWidget(helpToggleBtn_);
    rightLayout->addLayout(toolbarLayout);

    helpBox_ = new QTextBrowser(editorPane_);
    helpBox_->setReadOnly(true);
    helpBox_->setOpenLinks(false);
    helpBox_->setOpenExternalLinks(false);
    helpBox_->setFrameShape(QFrame::StyledPanel);
    helpBox_->setHtml(customTestsApiHelpHtml(editorFont_, QApplication::palette(helpBox_)));
    helpBox_->setMaximumHeight(230);
    helpBox_->setVisible(false);
    connect(themeManager(), &ThemeManager::themeChanged, this, [this] {
        if (helpBox_ != nullptr) helpBox_->setHtml(customTestsApiHelpHtml(editorFont_, QApplication::palette(helpBox_)));
    });
    rightLayout->addWidget(helpBox_);

    scriptEdit_ = new JsCodeEdit(editorPane_);
    scriptEdit_->applyMonospaceFont(editorFont);
    rightLayout->addWidget(scriptEdit_, 1);

    auto *checkRow = new QHBoxLayout();
    checkBtn_ = new QPushButton(tr("Check Syntax"), editorPane_);
    statusLabel_ = new QLabel(editorPane_);
    statusLabel_->setWordWrap(true);
    statusLabel_->setTextFormat(Qt::PlainText);
    checkRow->addWidget(checkBtn_);
    checkRow->addWidget(statusLabel_, 1);
    rightLayout->addLayout(checkRow);

    splitter->addWidget(leftWidget);
    splitter->addWidget(editorPane_);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 3);
    splitter->setSizes({210, 620});

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    rootLayout->addWidget(buttonBox);

    connect(buttonBox, &QDialogButtonBox::accepted, this, &DialogCustomTests::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(addBtn_, &QPushButton::clicked, this, &DialogCustomTests::addTest);
    connect(dupBtn_, &QPushButton::clicked, this, &DialogCustomTests::duplicateCurrentTest);
    connect(removeBtn_, &QPushButton::clicked, this, &DialogCustomTests::removeCurrentTest);
    connect(checkBtn_, &QPushButton::clicked, this, &DialogCustomTests::checkSyntax);
    connect(helpToggleBtn_, &QPushButton::toggled, helpBox_, &QWidget::setVisible);

    connect(list_, &QListWidget::currentRowChanged, this, [this](int row) {
        loadRow(row);
    });
    connect(list_, &ReorderListWidget::reorderRequested, this, [this](int from, int to) {
        if (from < 0 || from >= tests_.size() || to < 0 || to > tests_.size() || from == to) return;
        syncEditorState();
        const int targetIndex = from < to ? to - 1 : to;
        if (targetIndex == from || targetIndex < 0 || targetIndex >= tests_.size()) return;
        tests_.move(from, targetIndex);
        currentRow_ = -1;
        rebuildList(targetIndex);
    });

    connect(nameEdit_, &QLineEdit::textChanged, this, [this](const QString &text) {
        if (updatingUi_ || currentRow_ < 0 || currentRow_ >= tests_.size()) return;
        tests_[currentRow_].name = text;
        if (auto *item = list_->item(currentRow_)) {
            const QString display = text.trimmed().isEmpty() ? tr("(Untitled)") : text.trimmed();
            item->setText(display);
        }
    });
    connect(timeoutSpin_, &QSpinBox::valueChanged, this, [this](int value) {
        if (updatingUi_ || currentRow_ < 0 || currentRow_ >= tests_.size()) return;
        tests_[currentRow_].timeout_ms = value;
    });
    connect(scriptEdit_, &QPlainTextEdit::textChanged, this, [this] {
        if (updatingUi_ || currentRow_ < 0 || currentRow_ >= tests_.size()) return;
        tests_[currentRow_].script = scriptEdit_->toPlainText();
        statusLabel_->clear();
    });

    rebuildList(tests_.isEmpty() ? -1 : 0);
}

void DialogCustomTests::rebuildList(int selectRow) {
    updatingUi_ = true;
    list_->clear();
    for (const auto &test : tests_) {
        const QString display = test.name.trimmed().isEmpty() ? tr("(Untitled)") : test.name.trimmed();
        list_->addItem(display);
    }
    updatingUi_ = false;
    if (selectRow >= 0 && selectRow < list_->count()) {
        list_->setCurrentRow(selectRow);
    } else {
        loadRow(-1);
    }
}

void DialogCustomTests::loadRow(int row) {
    if (!updatingUi_) syncEditorState();
    currentRow_ = (row >= 0 && row < tests_.size()) ? row : -1;

    updatingUi_ = true;
    const bool hasRow = currentRow_ >= 0;
    editorPane_->setEnabled(hasRow);
    statusLabel_->clear();
    if (hasRow) {
        const auto &test = tests_[currentRow_];
        nameEdit_->setText(test.name);
        timeoutSpin_->setValue(test.timeout_ms > 0 ? test.timeout_ms : 5000);
        scriptEdit_->setPlainText(test.script);
    } else {
        nameEdit_->clear();
        timeoutSpin_->setValue(5000);
        scriptEdit_->clear();
    }
    updatingUi_ = false;
    updateButtonStates();
}

void DialogCustomTests::syncEditorState() {
    if (currentRow_ < 0 || currentRow_ >= tests_.size()) return;
    tests_[currentRow_].name = nameEdit_->text();
    tests_[currentRow_].timeout_ms = timeoutSpin_->value();
    tests_[currentRow_].script = scriptEdit_->toPlainText();
}

void DialogCustomTests::updateButtonStates() {
    const bool hasSelection = currentRow_ >= 0 && currentRow_ < tests_.size();
    dupBtn_->setEnabled(hasSelection);
    removeBtn_->setEnabled(hasSelection);
    checkBtn_->setEnabled(hasSelection);
}

QString DialogCustomTests::defaultTestName() const {
    const QString base = tr("Custom Test");
    for (int i = 1;; ++i) {
        const QString candidate = i == 1 ? base : QStringLiteral("%1 %2").arg(base).arg(i);
        bool used = false;
        for (const auto &t : tests_) {
            if (t.name.trimmed() == candidate) {
                used = true;
                break;
            }
        }
        if (!used) return candidate;
    }
}

void DialogCustomTests::addTest() {
    syncEditorState();
    Configs::CustomTest test;
    test.id = Configs::CustomTest::NewId();
    test.name = defaultTestName();
    test.script = Configs::CustomTest::DefaultScript();
    test.timeout_ms = 5000;
    tests_.append(test);
    const int newRow = static_cast<int>(tests_.size()) - 1;
    currentRow_ = -1;
    rebuildList(newRow);
    nameEdit_->setFocus();
    nameEdit_->selectAll();
}

void DialogCustomTests::duplicateCurrentTest() {
    if (currentRow_ < 0 || currentRow_ >= tests_.size()) return;
    syncEditorState();
    Configs::CustomTest copy = tests_[currentRow_];
    copy.id = Configs::CustomTest::NewId();
    copy.name = copy.name.trimmed().isEmpty() ? defaultTestName() : copy.name.trimmed() + tr(" (Copy)");
    const int insertRow = currentRow_ + 1;
    tests_.insert(insertRow, std::move(copy));
    currentRow_ = -1;
    rebuildList(insertRow);
}

void DialogCustomTests::removeCurrentTest() {
    if (currentRow_ < 0 || currentRow_ >= tests_.size()) return;
    const int row = currentRow_;
    tests_.removeAt(row);
    currentRow_ = -1;
    const int nextRow = tests_.isEmpty() ? -1 : qMin(row, static_cast<int>(tests_.size()) - 1);
    rebuildList(nextRow);
}

void DialogCustomTests::checkSyntax() {
    if (currentRow_ < 0 || currentRow_ >= tests_.size()) return;
    syncEditorState();
    bool rpcOK = false;
    const QString err = API::defaultClient->CheckCustomTest(&rpcOK, scriptEdit_->toPlainText());
    const auto &tokens = themeManager()->tokens;
    if (!rpcOK) {
        statusLabel_->setStyleSheet(QStringLiteral("color: %1;").arg(tokens.warning.name()));
        statusLabel_->setText(tr("Core is not connected."));
        return;
    }
    if (!err.isEmpty()) {
        statusLabel_->setStyleSheet(QStringLiteral("color: %1;").arg(tokens.danger.name()));
        statusLabel_->setText(err);
        return;
    }
    statusLabel_->setStyleSheet(QStringLiteral("color: %1;").arg(tokens.success.name()));
    statusLabel_->setText(tr("Syntax OK"));
}

void DialogCustomTests::accept() {
    syncEditorState();
    for (int i = 0; i < tests_.size(); ++i) {
        tests_[i].name = tests_[i].name.trimmed();
        if (tests_[i].id.isEmpty()) tests_[i].id = Configs::CustomTest::NewId();
        if (tests_[i].name.isEmpty()) {
            list_->setCurrentRow(i);
            QMessageBox::warning(this, windowTitle(), tr("Please enter a name for each custom test."));
            nameEdit_->setFocus();
            return;
        }
        if (tests_[i].script.trimmed().isEmpty()) {
            list_->setCurrentRow(i);
            QMessageBox::warning(this, windowTitle(), tr("Script cannot be empty."));
            scriptEdit_->setFocus();
            return;
        }
    }

    const auto error = Engine::SettingsService::Update([this](Configs::SettingsValues &values) {
        values.custom_tests = tests_;
    }, {.interactive = true});
    if (error) {
        QMessageBox::warning(this, windowTitle(), SettingsApplyErrorText(*error));
        return;
    }
    QDialog::accept();
}
