#pragma once

#include <QPushButton>

#include "include/database/entities/Profile.h"
#include "include/ui/utils/GuiUtils.hpp"

namespace Validation { class Form; }

class ProfileEditor {
public:
    virtual void onStart(std::shared_ptr<Configs::Profile> ent) = 0;

    virtual bool onEnd() = 0;

    // Called after onStart(); register fields and cross rules with owner = the editor widget.
    virtual void bindValidation(Validation::Form &form) {}

    std::function<QWidget *()> get_edit_dialog;
    std::function<QString()> get_edit_text_name;
    std::function<QString()> get_edit_text_serverAddress;
    std::function<QString()> get_edit_text_serverPort;
    std::function<void(const QString &)> set_edit_text_serverAddress;
    std::function<void(const QString &)> set_edit_text_serverPort;

    std::function<void()> editor_cache_updated;
    // Fire when the editor's own layout or one of the constraints below may have changed.
    std::function<void()> editor_state_changed;

    virtual QList<QPair<QPushButton *, QString>> get_editor_cached() { return {}; };

    // Constraints the editor's live input places on the outer dialog.
    virtual bool locksServerAddress() { return false; }

    virtual bool blocksMultiplex() { return false; }

    virtual bool usesQuic() { return false; }

    // The outer Port / Address may be left empty because the editor's own input supplies them (port ranges, realm, server lists).
    virtual bool serverPortOptional() { return false; }

    virtual bool serverAddressOptional() { return false; }
};
