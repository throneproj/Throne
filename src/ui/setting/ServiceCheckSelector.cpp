#include "include/ui/setting/ServiceCheckSelector.h"

#include "include/engine/tests/ServiceChecks.h"
#include "include/global/Utils.hpp"
#include "include/ui/utils/ServiceIcons.h"

#include <QCheckBox>
#include <QEvent>
#include <QGridLayout>
#include <QHash>
#include <QIcon>
#include <QLabel>
#include <QPointer>
#include <QSet>

namespace {
    constexpr int kServiceSelectorIconSize = 16;
    constexpr int kServiceSelectorColumns = 4;

    // The definitions name categories in English; the known ones get a translation.
    QString serviceSelectorCategoryName(const QString &id, const QString &name) {
        if (id == QLatin1String("ai")) return ServiceCheckSelector::tr("AI");
        if (id == QLatin1String("streaming")) return ServiceCheckSelector::tr("Streaming");
        if (id == QLatin1String("social")) return ServiceCheckSelector::tr("Social");
        if (id == QLatin1String("reputation")) return ServiceCheckSelector::tr("IP reputation");
        return name;
    }
}

ServiceCheckSelector::ServiceCheckSelector(const QStringList &disabledIds, QWidget *parent)
    : QWidget(parent), disabled_(disabledIds), grid_(new QGridLayout(this)) {
    grid_->setContentsMargins(0, 0, 0, 0);
    grid_->setColumnStretch(kServiceSelectorColumns + 1, 1);
    showMessage(tr("Loading services"));

    QPointer<ServiceCheckSelector> self(this);
    runOnNewThread([self] {
        const auto definitions = Engine::ServiceChecks::Current();
        runOnUiThread([self, definitions] {
            if (self) self->populate(definitions);
        });
    });
}

QStringList ServiceCheckSelector::disabledIds() const {
    if (!loaded_) return disabled_;
    QSet<QString> listed;
    QStringList unchecked;
    for (const auto &entry : entries_) {
        listed.insert(entry.id);
        if (!entry.box->isChecked()) unchecked << entry.id;
    }
    // Earlier choices keep their order, so an untouched list saves unchanged.
    QStringList ids;
    for (const auto &id : disabled_) {
        if (!listed.contains(id) || unchecked.contains(id)) ids << id;
    }
    for (const auto &id : unchecked) {
        if (!ids.contains(id)) ids << id;
    }
    return ids;
}

void ServiceCheckSelector::changeEvent(QEvent *event) {
    QWidget::changeEvent(event);
    // The monochrome logos take the text color, which a theme previewed in the same dialog changes.
    if (event->type() == QEvent::PaletteChange) refreshIcons();
}

void ServiceCheckSelector::populate(const Engine::ServiceChecks::Definitions &definitions) {
    if (!definitions.error.isEmpty()) {
        showMessage(tr("Cannot load services: %1").arg(definitions.error));
        return;
    }
    if (definitions.checks.isEmpty()) {
        showMessage(tr("No services available"));
        return;
    }
    delete message_;
    message_ = nullptr;

    QStringList order;
    QHash<QString, QString> labels;
    for (const auto &category : definitions.categories) {
        order << category.id;
        labels.insert(category.id, serviceSelectorCategoryName(category.id, category.name));
    }
    // A check whose category the definitions do not list still gets a row, the last one.
    if (!labels.contains(QString())) {
        order << QString();
        labels.insert(QString(), tr("Other"));
    }

    int row = 0;
    for (const auto &key : order) {
        int column = 0;
        for (const auto &check : definitions.checks) {
            if ((labels.contains(check.category) ? check.category : QString()) != key) continue;
            if (column == 0) {
                auto *label = new QLabel(labels.value(key), this);
                label->setTextFormat(Qt::PlainText);
                grid_->addWidget(label, row, 0);
            } else if (column % kServiceSelectorColumns == 0) {
                ++row;
            }
            auto *box = new QCheckBox(QString(check.name).replace(QLatin1Char('&'), QLatin1String("&&")), this);
            box->setIconSize(QSize(kServiceSelectorIconSize, kServiceSelectorIconSize));
            box->setChecked(!disabled_.contains(check.id));
            grid_->addWidget(box, row, 1 + column % kServiceSelectorColumns);
            entries_ << Entry{box, check.id, check.name};
            ++column;
        }
        if (column > 0) ++row;
    }
    refreshIcons();
    loaded_ = true;
}

void ServiceCheckSelector::showMessage(const QString &text) {
    if (message_ == nullptr) {
        message_ = new QLabel(this);
        message_->setTextFormat(Qt::PlainText);
        message_->setWordWrap(true);
        grid_->addWidget(message_, 0, 0, 1, kServiceSelectorColumns + 1);
    }
    message_->setText(text);
}

void ServiceCheckSelector::refreshIcons() {
    const QColor foreground = palette().color(QPalette::WindowText);
    for (const auto &entry : entries_) {
        entry.box->setIcon(QIcon(ServiceIcons::Pixmap(entry.id, entry.name, ServiceIcons::State::Plain, kServiceSelectorIconSize,
                                                      devicePixelRatioF(), foreground)));
    }
}
