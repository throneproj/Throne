#pragma once

#include <QWidget>

#include <memory>

#include "include/database/entities/RouteProfile.h"

class QLabel;

namespace RouteEditor {
    class ThenPill;
}

// An endpoint rule's panel, read-only: the Endpoints tab adds and removes the rule, the list only moves it.
class EndpointRulePanel : public QWidget {
    Q_OBJECT

public:
    explicit EndpointRulePanel(std::shared_ptr<Configs::RouteProfile> profile, QWidget* parent = nullptr);
    ~EndpointRulePanel() override;

    void setRule(const std::shared_ptr<Configs::RouteRule>& rule);

signals:
    void openEndpointsRequested();

private:
    std::shared_ptr<Configs::RouteProfile> profile;
    QLabel* title = nullptr;
    QLabel* subtitle = nullptr;
    QLabel* text = nullptr;
    RouteEditor::ThenPill* pill = nullptr;
};
