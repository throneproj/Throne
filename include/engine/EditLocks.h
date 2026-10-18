#pragma once

namespace Engine {
    // What an open editor in the window saves back on OK; the API refuses to change it meanwhile. Main thread only.
    enum class EditTarget {
        Settings,
        Routes,
        Group,
        Profile,
    };

    class EditLock {
    public:
        // id -1 covers every id of the target.
        explicit EditLock(EditTarget target, int id = -1);
        ~EditLock();

        EditLock(const EditLock &) = delete;
        EditLock &operator=(const EditLock &) = delete;

    private:
        EditTarget target_;
        int id_;
    };

    bool IsBeingEdited(EditTarget target, int id = -1);

    bool AnyEditorOpen();
} // namespace Engine
