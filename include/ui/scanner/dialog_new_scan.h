#pragma once

#include <QDialog>

class QLineEdit;
class QRadioButton;

namespace Validation {
    class Form;
}

class DialogNewScan : public QDialog {
    Q_OBJECT

public:
    explicit DialogNewScan(QWidget *parent = nullptr);

    [[nodiscard]] int CreatedScanId() const { return createdId; }

    void accept() override;

private:
    QLineEdit *nameEdit;
    QRadioButton *genericRadio;
    QRadioButton *warpRadio;
    QRadioButton *resolverRadio;
    QRadioButton *tuneRadio;
    Validation::Form *form;
    int createdId = -1;
};
