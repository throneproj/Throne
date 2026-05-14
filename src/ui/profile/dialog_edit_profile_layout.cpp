#include "include/ui/profile/dialog_edit_profile.h"

#include <QBoxLayout>
#include <QGuiApplication>
#include <QResizeEvent>
#include <QScreen>
#include <QScrollBar>
#include <QShowEvent>
#include <QSizePolicy>
#include <QSpacerItem>

namespace {
constexpr int kScreenMargin = 24;
constexpr int kLeftColumnMinimumWidth = 400;
constexpr int kRightColumnMinimumWidth = 400;
constexpr int kXrayColumnMinimumWidth = 860;
constexpr int kXHTTPScrollHeightExtra = 48;
constexpr int kOldEditorBaselineWidth = 1307;

QSize widgetNaturalSize(QWidget *widget) {
    if (auto *layout = widget->layout()) {
        layout->invalidate();
        layout->activate();
    }
    return widget->sizeHint()
        .expandedTo(widget->minimumSizeHint())
        .expandedTo(widget->minimumSize());
}

void setColumnMinimumWidth(QWidget *widget, int minimumWidth) {
    widget->setMinimumWidth(widget->isHidden() ? 0 : minimumWidth);
    widget->setMaximumWidth(QWIDGETSIZE_MAX);
}

void configureTrailingSpacer(QBoxLayout *layout, bool enabled) {
    if (!layout || layout->count() <= 0) return;
    const auto spacerIndex = layout->count() - 1;
    layout->setStretch(spacerIndex, enabled ? 1 : 0);
    if (auto *item = layout->itemAt(spacerIndex)) {
        if (auto *spacer = item->spacerItem()) {
            spacer->changeSize(enabled ? 40 : 0, 20,
                               enabled ? QSizePolicy::Expanding : QSizePolicy::Fixed,
                               QSizePolicy::Minimum);
        }
    }
}

bool layoutWidgetShown(const QWidget *widget) {
    return widget && !widget->isHidden();
}

bool layoutXHTTPShown(const Ui::DialogEditProfile *ui) {
    return layoutWidgetShown(ui->xray_widget) &&
           layoutWidgetShown(ui->xray_network_box) &&
           layoutWidgetShown(ui->xray_xhttp_box);
}

QList<QWidget *> layoutRightContentBlocks(const Ui::DialogEditProfile *ui) {
    return {
        ui->xray_security_box,
        ui->xray_network_box,
        ui->xray_xhttp_box,
        ui->xray_xhttp_params_box,
        ui->xray_xhttp_padding_box,
        ui->xray_xhttp_xmux_box,
    };
}

void setMaximumVerticalPolicy(QWidget *widget) {
    auto policy = widget->sizePolicy();
    policy.setVerticalPolicy(QSizePolicy::Maximum);
    widget->setSizePolicy(policy);
}

void setExpandingMaximumPolicy(QWidget *widget) {
    auto policy = widget->sizePolicy();
    policy.setHorizontalPolicy(QSizePolicy::Expanding);
    policy.setVerticalPolicy(QSizePolicy::Maximum);
    widget->setSizePolicy(policy);
}
}

void DialogEditProfile::setupDialogLayoutBehavior() {
    ui->dialog_scroll_area->setWidgetResizable(true);
    ui->dialog_scroll_contents->setMinimumSize(0, 0);
    ui->dialog_scroll_area->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    ui->dialog_scroll_contents->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    ui->left_w->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Maximum);
    ui->right_all_w->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    ui->xray_widget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    for (auto *widget : layoutRightContentBlocks(ui)) {
        setMaximumVerticalPolicy(widget);
    }

    ui->dialog_layout->setAlignment(ui->left_w, Qt::AlignTop);
    ui->dialog_layout->setAlignment(ui->right_all_w, Qt::AlignVCenter);
    ui->dialog_layout->setAlignment(ui->xray_widget, Qt::AlignVCenter);
    ui->dialog_layout->setStretchFactor(ui->left_w, 0);
    ui->dialog_layout->setStretchFactor(ui->right_all_w, 0);
    ui->dialog_layout->setStretchFactor(ui->xray_widget, 0);
    configureTrailingSpacer(ui->dialog_layout, true);

    xrayLayoutBaseMargins = ui->verticalLayout_5->contentsMargins();
    xrayTopSpacer = new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Fixed);
    ui->verticalLayout_5->insertSpacerItem(0, xrayTopSpacer);
    ui->left->setAlignment(Qt::AlignTop);
    ui->right_layout->setAlignment(Qt::AlignTop);
    ui->verticalLayout_5->setAlignment(Qt::AlignTop);
    ui->verticalLayout_8->setAlignment(Qt::AlignTop);
    ui->verticalLayout_10->setAlignment(Qt::AlignTop);
}

void DialogEditProfile::updateDialogColumnSizing() {
    const auto isXHTTP = layoutXHTTPShown(ui);
    const auto hasRightColumn = layoutWidgetShown(ui->right_all_w);
    const auto hasXrayColumn = layoutWidgetShown(ui->xray_widget);
    const auto xrayMinimumWidth = isXHTTP ? 0 : kXrayColumnMinimumWidth;

    setColumnMinimumWidth(ui->left_w, kLeftColumnMinimumWidth);
    setColumnMinimumWidth(ui->right_all_w, kRightColumnMinimumWidth);
    setColumnMinimumWidth(ui->xray_widget, xrayMinimumWidth);

    auto rightPolicy = ui->right_all_w->sizePolicy();
    rightPolicy.setHorizontalPolicy(hasRightColumn && !hasXrayColumn
                                        ? QSizePolicy::Expanding
                                        : QSizePolicy::Preferred);
    ui->right_all_w->setSizePolicy(rightPolicy);

    auto xrayPolicy = ui->xray_widget->sizePolicy();
    xrayPolicy.setHorizontalPolicy(hasXrayColumn ? QSizePolicy::Expanding : QSizePolicy::Preferred);
    xrayPolicy.setVerticalPolicy(isXHTTP ? QSizePolicy::Maximum : QSizePolicy::Preferred);
    ui->xray_widget->setSizePolicy(xrayPolicy);

    for (auto *widget : layoutRightContentBlocks(ui)) {
        setExpandingMaximumPolicy(widget);
    }

    const auto rightStretch = hasRightColumn && !hasXrayColumn ? 1 : 0;
    const auto xrayStretch = hasXrayColumn ? 1 : 0;
    ui->dialog_layout->setStretchFactor(ui->left_w, 0);
    ui->dialog_layout->setStretchFactor(ui->right_all_w, rightStretch);
    ui->dialog_layout->setStretchFactor(ui->xray_widget, xrayStretch);
    configureTrailingSpacer(ui->dialog_layout, !hasRightColumn && !hasXrayColumn);

    ui->dialog_layout->setAlignment(ui->left_w, Qt::AlignTop);
    ui->dialog_layout->setAlignment(ui->right_all_w, Qt::AlignVCenter);
    ui->dialog_layout->setAlignment(ui->xray_widget, isXHTTP ? Qt::AlignTop : Qt::AlignVCenter);
    ui->verticalLayout_5->setAlignment(Qt::AlignTop);
    ui->verticalLayout_5->invalidate();
}

void DialogEditProfile::updateXrayVerticalOffset() {
    auto margins = xrayLayoutBaseMargins;
    const auto setTopSpacerHeight = [this](int height) {
        if (!xrayTopSpacer) return;
        xrayTopSpacer->changeSize(0, height, QSizePolicy::Minimum, QSizePolicy::Fixed);
    };
    if (ui->xray_widget->isHidden() || layoutXHTTPShown(ui)) {
        ui->verticalLayout_5->setContentsMargins(margins);
        setTopSpacerHeight(0);
        ui->verticalLayout_5->invalidate();
        ui->verticalLayout_5->activate();
        ui->xray_widget->updateGeometry();
        return;
    }

    const QList<QWidget *> xrayBlocks = {ui->xray_security_box, ui->xray_network_box};
    int visibleBlocks = 0;
    int rightContentHeight = margins.top() + margins.bottom();
    const auto spacing = qMax(0, ui->verticalLayout_5->spacing());
    for (auto *block : xrayBlocks) {
        if (block->isHidden()) continue;
        if (visibleBlocks > 0) rightContentHeight += spacing;
        rightContentHeight += widgetNaturalSize(block).height();
        visibleBlocks++;
    }

    if (visibleBlocks == 0) {
        ui->verticalLayout_5->setContentsMargins(margins);
        setTopSpacerHeight(0);
        ui->verticalLayout_5->invalidate();
        ui->verticalLayout_5->activate();
        ui->xray_widget->updateGeometry();
        return;
    }

    const auto leftHeight = qMax(ui->left_w->height(), widgetNaturalSize(ui->left_w).height());
    const auto viewportHeight = ui->dialog_scroll_area->viewport() ? ui->dialog_scroll_area->viewport()->height() : 0;
    const auto targetHeight = qMax(qMax(leftHeight, viewportHeight), rightContentHeight);
    const auto topOffset = qMax(0, (targetHeight - rightContentHeight) / 2);
    ui->verticalLayout_5->setContentsMargins(margins);
    setTopSpacerHeight(topOffset);
    ui->xray_widget->setMinimumHeight(targetHeight);
    ui->xray_widget->setMaximumHeight(targetHeight);
    ui->verticalLayout_5->invalidate();
    ui->verticalLayout_5->activate();
    ui->xray_widget->updateGeometry();
}

QSize DialogEditProfile::visibleDialogContentSize() const {
    const auto margins = ui->dialog_layout->contentsMargins();
    const auto spacing = qMax(0, ui->dialog_layout->spacing());
    const QList<QWidget *> columns = {ui->left_w, ui->right_all_w, ui->xray_widget};

    int width = margins.left() + margins.right();
    int height = margins.top() + margins.bottom();
    int visibleColumns = 0;
    int maxColumnHeight = 0;

    for (auto *column : columns) {
        if (!layoutWidgetShown(column)) continue;

        const auto columnSize = widgetNaturalSize(column);
        if (visibleColumns > 0) width += spacing;
        width += columnSize.width();
        maxColumnHeight = qMax(maxColumnHeight, columnSize.height());
        visibleColumns++;
    }

    height += maxColumnHeight;
    return QSize(width, height);
}

void DialogEditProfile::refreshDialogLayout() {
    ui->verticalLayout_5->setContentsMargins(xrayLayoutBaseMargins);

    const auto contentHeightWidgets = layoutRightContentBlocks(ui);
    for (auto *widget : contentHeightWidgets) {
        widget->setMinimumHeight(0);
        widget->setMaximumHeight(QWIDGETSIZE_MAX);
    }
    ui->xray_widget->setMinimumHeight(0);
    ui->xray_widget->setMaximumHeight(QWIDGETSIZE_MAX);

    for (int pass = 0; pass < 2; ++pass) {
        const auto xhttpLayoutChanged = updateXrayXHTTPResponsiveLayout();
        updateDialogColumnSizing();

        if (auto *layout = ui->dialog_scroll_contents->layout()) {
            ui->dialog_scroll_contents->setMinimumSize(0, 0);
            layout->invalidate();
            layout->activate();
        }
        if (!xhttpLayoutChanged) break;
    }

    for (auto *widget : contentHeightWidgets) {
        if (!layoutWidgetShown(widget)) continue;
        if (auto *layout = widget->layout()) layout->activate();
        const auto height = widget->sizeHint().expandedTo(widget->minimumSizeHint()).height();
        widget->setMinimumHeight(height);
        widget->setMaximumHeight(height);
    }

    updateDialogColumnSizing();
    if (!ui->xray_xhttp_box->isVisible() && !ui->xray_widget->isHidden()) {
        if (auto *layout = ui->xray_widget->layout()) {
            layout->invalidate();
            layout->activate();
        }
        const auto height = ui->xray_widget->sizeHint().expandedTo(ui->xray_widget->minimumSizeHint()).height();
        ui->xray_widget->setMinimumHeight(height);
        ui->xray_widget->setMaximumHeight(height);
    }

    updateXrayVerticalOffset();

    if (auto *layout = ui->dialog_scroll_contents->layout()) {
        layout->invalidate();
        layout->activate();
    }
    const auto contentSize = visibleDialogContentSize();
    ui->dialog_scroll_contents->setMinimumSize(contentSize);
    ui->dialog_scroll_contents->updateGeometry();
    ui->dialog_scroll_area->updateGeometry();
}

void DialogEditProfile::fitDialogToContent() {
    refreshDialogLayout();

    auto *targetScreen = screen() ? screen() : QGuiApplication::primaryScreen();
    if (!targetScreen) {
        adjustSize();
        adjustPosition(mainwindow);
        return;
    }

    const auto available = targetScreen->availableGeometry();
    const auto rootMargins = ui->root_layout->contentsMargins();
    const auto contentSize = visibleDialogContentSize();
    const auto buttonsHint = ui->buttonBox->sizeHint();
    const auto vScrollWidth = ui->dialog_scroll_area->verticalScrollBar()
                                  ? ui->dialog_scroll_area->verticalScrollBar()->sizeHint().width()
                                  : 0;
    const auto rootSpacing = qMax(0, ui->root_layout->spacing());
    const auto isXHTTP = layoutXHTTPShown(ui);

    const auto columnSize = [](QWidget *widget) {
        if (!layoutWidgetShown(widget)) return QSize();
        return widgetNaturalSize(widget);
    };
    const auto leftSize = columnSize(ui->left_w);
    const auto rightSize = columnSize(ui->right_all_w);
    const auto xraySize = columnSize(ui->xray_widget);
    const int visibleColumnCount = (layoutWidgetShown(ui->left_w) ? 1 : 0) +
                                   (layoutWidgetShown(ui->right_all_w) ? 1 : 0) +
                                   (layoutWidgetShown(ui->xray_widget) ? 1 : 0);
    const auto hasOnlyLeftColumn = visibleColumnCount == 1 && layoutWidgetShown(ui->left_w);
    const auto hasMultipleColumns = visibleColumnCount > 1;

    int targetWidth = contentSize.width() + rootMargins.left() + rootMargins.right() + vScrollWidth + 8;
    if (hasOnlyLeftColumn) {
        targetWidth = leftSize.width() + rootMargins.left() + rootMargins.right() + vScrollWidth + 8;
    } else if (hasMultipleColumns && !isXHTTP) {
        targetWidth = qMax(targetWidth, kOldEditorBaselineWidth);
    }
    targetWidth = qMax(targetWidth, buttonsHint.width() + rootMargins.left() + rootMargins.right());
    targetWidth = qMin(targetWidth, available.width() - kScreenMargin);

    int targetScrollHeight = contentSize.height();
    if (isXHTTP && !ui->left_w->isHidden()) {
        targetScrollHeight = qMin(contentSize.height(), leftSize.height() + kXHTTPScrollHeightExtra);
    } else {
        targetScrollHeight = qMax(qMax(leftSize.height(), rightSize.height()),
                                  qMax(xraySize.height(), contentSize.height()));
    }

    int targetHeight = targetScrollHeight + buttonsHint.height() + rootMargins.top() +
                       rootMargins.bottom() + rootSpacing + 8;
    targetHeight = qMin(targetHeight, available.height() - kScreenMargin);

    resize(qMax(minimumWidth(), targetWidth), qMax(minimumHeight(), targetHeight));
    refreshDialogLayout();
    adjustPosition(mainwindow);
}

void DialogEditProfile::resizeEvent(QResizeEvent *event) {
    QDialog::resizeEvent(event);
    if (!postShowFitDone) return;
    refreshDialogLayout();
}

void DialogEditProfile::showEvent(QShowEvent *event) {
    QDialog::showEvent(event);
    if (postShowFitDone) return;
    postShowFitDone = true;
    fitDialogToContent();
}
