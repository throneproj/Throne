#pragma once

#include <QPointer>
#include <QWidget>

#include <functional>

#include "include/configs/common/multiplex.h"
#include "include/configs/common/xrayMultiplex.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;

namespace Validation {
    class Form;
}

// Editor for one sing-box multiplex setup: the Preset Settings tab and a profile's Custom multiplex.
class MuxSettingsWidget : public QWidget {
    Q_OBJECT

public:
    explicit MuxSettingsWidget(QWidget *parent = nullptr);

    void setSettings(const Configs::MuxSettings &settings);
    [[nodiscard]] Configs::MuxSettings settings() const;
    // labelPrefix leads every field name in the form's messages.
    void bindValidation(Validation::Form *form, const QString &labelPrefix = {});

    // A small window-modal dialog over parent; done runs only when it is accepted.
    static void openDialog(QWidget *parent, const Configs::MuxSettings &settings,
                           std::function<void(const Configs::MuxSettings &)> done);

private:
    void updateControls();

    QPointer<Validation::Form> m_form;

    QComboBox *m_protocol = nullptr;
    QLabel *m_modeLabel = nullptr;
    QComboBox *m_mode = nullptr;
    QLabel *m_maxStreamsLabel = nullptr;
    QLineEdit *m_maxStreams = nullptr;
    QLabel *m_maxConnectionsLabel = nullptr;
    QLineEdit *m_maxConnections = nullptr;
    QLabel *m_minStreamsLabel = nullptr;
    QLineEdit *m_minStreams = nullptr;
    QCheckBox *m_padding = nullptr;
    QCheckBox *m_brutal = nullptr;
    QLabel *m_brutalUpLabel = nullptr;
    QLineEdit *m_brutalUp = nullptr;
    QLabel *m_brutalDownLabel = nullptr;
    QLineEdit *m_brutalDown = nullptr;
};

// Editor for one Xray mux setup: the Preset Settings tab and a profile's Custom mux.
class XrayMuxSettingsWidget : public QWidget {
    Q_OBJECT

public:
    explicit XrayMuxSettingsWidget(QWidget *parent = nullptr);

    void setSettings(const Configs::XrayMuxSettings &settings);
    [[nodiscard]] Configs::XrayMuxSettings settings() const;
    // labelPrefix leads every field name in the form's messages.
    void bindValidation(Validation::Form *form, const QString &labelPrefix = {});

    // A small window-modal dialog over parent; done runs only when it is accepted.
    static void openDialog(QWidget *parent, const Configs::XrayMuxSettings &settings,
                           std::function<void(const Configs::XrayMuxSettings &)> done);

private:
    QLabel *m_concurrencyLabel = nullptr;
    QLineEdit *m_concurrency = nullptr;
    QLabel *m_maxConnectionsLabel = nullptr;
    QLineEdit *m_maxConnections = nullptr;
    QLabel *m_xudpConcurrencyLabel = nullptr;
    QLineEdit *m_xudpConcurrency = nullptr;
    QComboBox *m_xudpProxyUDP443 = nullptr;
};
