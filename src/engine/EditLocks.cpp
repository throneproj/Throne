#include "include/engine/EditLocks.h"

#include <QHash>
#include <QPair>

#include "include/engine/ThreadCheck.h"

namespace Engine {
    namespace {
        QHash<QPair<int, int>, int> &editLockCounts() {
            static QHash<QPair<int, int>, int> counts;
            return counts;
        }

        QPair<int, int> editLockKey(EditTarget target, int id) {
            return {static_cast<int>(target), id};
        }
    } // namespace

    EditLock::EditLock(EditTarget target, int id) : target_(target), id_(id) {
        ENGINE_CHECK_MAIN_THREAD();
        ++editLockCounts()[editLockKey(target_, id_)];
    }

    EditLock::~EditLock() {
        auto &counts = editLockCounts();
        const auto key = editLockKey(target_, id_);
        if (--counts[key] <= 0) counts.remove(key);
    }

    bool IsBeingEdited(EditTarget target, int id) {
        ENGINE_CHECK_MAIN_THREAD();
        const auto &counts = editLockCounts();
        if (counts.contains(editLockKey(target, -1))) return true;
        if (id >= 0) return counts.contains(editLockKey(target, id));
        for (auto it = counts.cbegin(); it != counts.cend(); ++it) {
            if (it.key().first == static_cast<int>(target)) return true;
        }
        return false;
    }

    bool AnyEditorOpen() {
        ENGINE_CHECK_MAIN_THREAD();
        return !editLockCounts().isEmpty();
    }
} // namespace Engine
