#pragma once

#include <QHash>
#include <QList>
#include <QPointer>
#include <QSet>
#include <QStringList>
#include <QWidget>

#include <memory>

#include "include/database/entities/RouteRule.h"

class NoWheelComboBox;
class QCheckBox;
class QLabel;
class QLayout;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QToolButton;
class QVBoxLayout;
class RouteThenPicker;

namespace RouteEditor {
    class FlowLayout;
    class TreeNode;
    class ValueChip;
} // namespace RouteEditor

// A custom rule's editor: its name, the condition tree, Then with the action's settings, and the JSON drawer.
class RouteRuleEditor : public QWidget {
    Q_OBJECT

public:
    explicit RouteRuleEditor(QWidget* parent = nullptr);
    ~RouteRuleEditor() override;

    // A condition added but not filled yet is dropped when another rule is loaded.
    void setRule(const std::shared_ptr<Configs::RouteRule>& rule);
    [[nodiscard]] const std::shared_ptr<Configs::RouteRule>& rule() const { return current; }

    // Focuses the name with its text selected, as for a rule just created.
    void focusName();

    // Re-reads the rule after something else changed it, such as turning it off.
    void refresh();

    // Opens what edits attr and returns the widget to focus: a value's chip, a condition's line, a setting or the Then picker.
    QWidget* reveal(const QString& attr, const QString& value);

    // Typed values that failed their check and are still in a condition's field; an empty message when there are none.
    [[nodiscard]] Configs::RuleIssue pendingIssue() const;

signals:
    // The rule was edited.
    void changed();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    // A condition's leaf in the tree, with the issues label under it.
    struct Line {
        QWidget* host = nullptr;
        RouteEditor::FlowLayout* flow = nullptr;
        QWidget* add = nullptr;
        QList<RouteEditor::ValueChip*> chips;
        QLabel* issues = nullptr;
    };

    // The inline field of one condition line, re-created whenever its line is refilled.
    struct Inline {
        bool open = false;
        QString attr;
        bool replacing = false;
        // A text box holding every value of the condition, one per line; committing replaces them all.
        bool asText = false;
        QString original;
        // Carried into the re-created field: what the user typed, or the lines that failed.
        QString text;
        QString error;
        // The values the text box marks as failed.
        QStringList failed;
        QPointer<QLineEdit> edit;
        QPointer<QPlainTextEdit> box;
    };

    // A setting row of the action's settings or the Connection options, with the issues of its attributes.
    struct Setting {
        QStringList attrs;
        QLabel* issues = nullptr;
    };

    // Scoped settings a Then change cleared, put back when an action that uses them is picked again.
    struct Stash {
        std::shared_ptr<Configs::RouteRule> rule;
        QHash<QString, QStringList> values;
        // The fallback delay a Fragment TLS choice without the handshake cleared.
        QString fragmentDelay;
    };

    void buildThenSection(QVBoxLayout* layout);
    // Each adds its toggle to toggles and its drawer to layout.
    void buildOptions(QVBoxLayout* layout, QLayout* toggles);
    void buildJson(QVBoxLayout* layout, QLayout* toggles);
    QLabel* addSettingIssues(const QStringList& attrs, QWidget* parent);

    void loadRule();
    void loadThenSection();
    void loadFragment();
    [[nodiscard]] QStringList shownAttrs() const;
    void rebuildBoxes();
    // labels: those of the leaf's siblings, which share one width.
    void buildLeaf(RouteEditor::TreeNode* node, const QString& attr, const QStringList& labels);
    void fillLine(const QString& attr);
    QLineEdit* makeInlineEdit(const QString& attr, QWidget* host);
    QPlainTextEdit* makeTextBox(const QString& attr, QWidget* host);
    [[nodiscard]] QWidget* inlineWidget() const;
    [[nodiscard]] QString inlineText() const;
    void focusInlineWidget();
    // Stops watching the inline field before it goes; keepText carries its text into the next one.
    void detachInline(bool keepText);
    void scheduleRefill(const QString& attr);
    // Waits while a mouse button is held, so the pressed button outlives the rebuild until its click lands.
    void runScheduled();
    void applyScheduled();
    void focusLine(const QString& attr, int index);

    // Commits or drops the open field before another opens; false when its values failed and it stays open.
    bool closeInline();
    void startInline(const QString& attr, const QString& value, bool replacing);
    void startText(const QString& attr);
    void commitInline(bool keepOpen, bool focusAfter, const QString& extra = {});
    void cancelInline(bool focusAfter);
    void openLeafMenu(const QString& attr, const QPoint& globalPos, const QString& value);
    void removeValue(const QString& attr, const QString& value);
    void openSelectMenu(const QString& attr, QWidget* anchor);
    void setSelectValue(const QString& attr, const QString& value);
    void addCondition(const QString& attr);
    void chooseApps(const QString& attr);

    void onThenPicked();
    void applyStash();
    void setScopedText(const QString& attr, const QString& text);
    void setScopedFlag(const QString& attr, bool on);
    void setFragment(bool handshake, bool records);
    void setOptionsOpen(bool open);
    void setJsonOpen(bool open);
    void refreshIssues();
    void refreshJson();
    void emitChanged();

    std::shared_ptr<Configs::RouteRule> current;
    bool loading = false;

    QLineEdit* nameEdit = nullptr;
    RouteEditor::TreeNode* tree = nullptr;
    QPushButton* addConditionButton = nullptr;
    QCheckBox* invertCheck = nullptr;

    RouteThenPicker* thenPicker = nullptr;
    QLabel* thenIssues = nullptr;
    QWidget* extrasFrame = nullptr;
    QWidget* rejectGroup = nullptr;
    NoWheelComboBox* rejectCombo = nullptr;
    QCheckBox* noDropCheck = nullptr;
    QWidget* sniffGroup = nullptr;
    QCheckBox* sniffOverrideCheck = nullptr;
    QWidget* resolveGroup = nullptr;
    NoWheelComboBox* strategyCombo = nullptr;

    QWidget* optionsFrame = nullptr;
    QToolButton* optionsToggle = nullptr;
    QLineEdit* overrideAddressEdit = nullptr;
    QLineEdit* overridePortEdit = nullptr;
    NoWheelComboBox* fragmentCombo = nullptr;
    QLineEdit* fragmentDelayEdit = nullptr;
    QLineEdit* spoofEdit = nullptr;
    NoWheelComboBox* spoofMethodCombo = nullptr;

    QToolButton* jsonToggle = nullptr;
    QPlainTextEdit* jsonView = nullptr;

    QHash<QString, QWidget*> settingWidgets;
    QList<Setting> settings;

    QStringList pending;
    QStringList builtAttrs;
    QHash<QString, Line> lines;
    Inline inlineState;
    // Long conditions shown whole instead of folded.
    QSet<QString> expanded;

    QSet<QString> refillAttrs;
    bool applyQueued = false;
    int releaseWaits = 0;
    bool focusInline = false;
    QString focusAttr;
    int focusIndex = -1;
    QString menuAttr;

    QHash<const Configs::RouteRule*, Stash> stashes;
};
