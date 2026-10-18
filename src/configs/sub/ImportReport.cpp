#include "include/configs/sub/ImportReport.hpp"

#include "include/global/Utils.hpp"

#include <QCoreApplication>
#include <QMessageBox>
#include <QPointer>

#include <algorithm>

namespace Subscription {
    namespace {
        constexpr qsizetype kImportReportExamples = 3;
        constexpr qsizetype kImportReportValueChars = 64;

        QString importReportElide(const QString &text) {
            return text.size() > kImportReportValueChars ? text.left(kImportReportValueChars) + QChar(0x2026) : text;
        }
    }

    void ImportReport::add(const ParseReport &report) {
        if (!report.document) {
            failedCount += !report.imported;
            warnedCount += report.imported && !report.issues.isEmpty();
        }
        for (const auto &issue : report.issues) {
            // A document-level error is why the import fell short, so it joins the summary's reasons.
            if (report.document && issue.severity == Validation::Severity::Error) {
                const auto text = issue.field.isEmpty() ? issue.message : issue.field + ": " + issue.message;
                addFatal(report.source.isEmpty() ? text : report.source + ": " + text);
                continue;
            }
            const auto key = QString::number(static_cast<int>(issue.severity)) + QChar(0x1f) + issue.field + QChar(0x1f) + issue.message;
            auto it = groupIndex.constFind(key);
            if (it == groupIndex.constEnd()) {
                it = groupIndex.insert(key, groups.size());
                groups.append(Group{issue.severity, issue.field, issue.message});
            }
            auto &group = groups[*it];
            ++group.count;
            if (group.examples.size() >= kImportReportExamples) continue;
            QString example = report.source;
            if (!issue.value.isEmpty()) {
                const auto value = "\"" + importReportElide(issue.value) + "\"";
                example = example.isEmpty() ? value : example + " (" + value + ")";
            }
            if (!example.isEmpty() && !group.examples.contains(example)) group.examples << example;
        }
    }

    void ImportReport::addFatal(const QString &reason) {
        const auto text = reason.trimmed();
        if (!text.isEmpty() && !fatal.contains(text)) fatal << text;
    }

    void ImportReport::setImported(int count) {
        importedCount = count;
    }

    bool ImportReport::hasProblems() const {
        return failedCount > 0 || warnedCount > 0 || !fatal.isEmpty() || !groups.isEmpty();
    }

    bool ImportReport::hasErrors() const {
        if (failedCount > 0 || !fatal.isEmpty()) return true;
        return std::any_of(groups.cbegin(), groups.cend(), [](const Group &g) { return g.severity == Validation::Severity::Error; });
    }

    QString ImportReport::summary() const {
        QString text = QCoreApplication::translate("Validation", "Imported %n profile(s).", nullptr, importedCount);
        if (failedCount > 0) text += ' ' + QCoreApplication::translate("Validation", "%n could not be imported.", nullptr, failedCount);
        if (warnedCount > 0) text += ' ' + QCoreApplication::translate("Validation", "%n imported with warnings.", nullptr, warnedCount);
        for (const auto &reason : fatal) text += '\n' + reason;
        return text;
    }

    QString ImportReport::details(int maxLines) const {
        QStringList rendered;
        const auto line = [](const Group &g) {
            QString text = g.field.isEmpty() ? g.message : g.field + ": " + g.message;
            const auto count = QString::number(g.count);
            const auto examples = g.examples.join(", ");
            if (g.count > 1 && !g.examples.isEmpty()) {
                text = QCoreApplication::translate("Validation", "%1 (%2×, e.g. %3)").arg(text, count, examples);
            } else if (g.count > 1) {
                text = QCoreApplication::translate("Validation", "%1 (%2×)").arg(text, count);
            } else if (!g.examples.isEmpty()) {
                text = QCoreApplication::translate("Validation", "%1 (%2)").arg(text, examples);
            }
            const auto tag = g.severity == Validation::Severity::Error ? QCoreApplication::translate("Validation", "[error]")
                                                                        : QCoreApplication::translate("Validation", "[warning]");
            return tag + ' ' + text;
        };
        for (const auto &group : lines()) {
            if (rendered.size() >= maxLines) break;
            rendered << line(group);
        }
        if (const auto more = groups.size() - rendered.size(); more > 0) {
            rendered << QCoreApplication::translate("Validation", "…and %n more", nullptr, static_cast<int>(more));
        }
        return rendered.join('\n');
    }

    QList<ImportReport::Line> ImportReport::lines() const {
        QList<Line> result;
        result.reserve(groups.size());
        for (const auto severity : {Validation::Severity::Error, Validation::Severity::Warning}) {
            for (const auto &group : groups) {
                if (group.severity == severity) result << group;
            }
        }
        return result;
    }

    void ShowImportReport(const QString &title, const ImportReport &report) {
        if (!IsGuiApp()) {
            QString text = title + '\n' + report.summary();
            if (const auto details = report.details(); !details.isEmpty()) text += '\n' + details;
            MW_show_log(text);
            return;
        }
        auto *app = QCoreApplication::instance();
        QMetaObject::invokeMethod(app, [title, report] {
            static QHash<QString, QPointer<QMessageBox>> boxes;
            const auto details = report.details();
            auto text = report.summary();
            if (!details.isEmpty()) text += "\n\n" + QCoreApplication::translate("Validation", "See the details below.");
            const auto icon = report.imported() == 0 || report.hasErrors() ? QMessageBox::Warning : QMessageBox::Information;

            auto &box = boxes[title];
            if (box) {
                box->setIcon(icon);
                box->setText(text);
                box->setDetailedText(details);
                box->raise();
                return;
            }
            // Not the active window: a transient dialog would take the box down with it.
            QWidget *parent = mainwindow != nullptr && mainwindow->isVisible() ? mainwindow : nullptr;
            box = new QMessageBox(icon, title, text, QMessageBox::Ok, parent);
            // Profile names and server messages are foreign text: AutoText would render a tag-like one as HTML.
            box->setTextFormat(Qt::PlainText);
            box->setDetailedText(details);
            box->setAttribute(Qt::WA_DeleteOnClose);
            box->setWindowModality(Qt::NonModal);
            box->show();
        }, Qt::QueuedConnection);
    }
}
