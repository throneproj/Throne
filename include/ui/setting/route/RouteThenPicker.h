#pragma once

#include <QPushButton>

#include <memory>

#include "include/database/entities/RouteRule.h"

// A custom rule's Then: a button showing the rule's pill, whose popup picks an action or a profile.
class RouteThenPicker : public QPushButton {
    Q_OBJECT

public:
    explicit RouteThenPicker(QWidget* parent = nullptr);

    void setRule(const std::shared_ptr<Configs::RouteRule>& rule);

    // After the rule's action or outbound changed elsewhere.
    void refresh();

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

signals:
    // The pick is already written into the rule with SetThen.
    void picked();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    void openPopup();
    [[nodiscard]] QSize contentSize(bool minimum) const;

    std::shared_ptr<Configs::RouteRule> rule;
};
