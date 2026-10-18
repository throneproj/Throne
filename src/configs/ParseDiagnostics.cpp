#include "include/configs/ParseDiagnostics.h"

#include <utility>

namespace Configs::ParseDiag {
    namespace {
        thread_local Scope *parseDiagCurrentScope = nullptr;
    }

    Scope::Scope() : previous(parseDiagCurrentScope) {
        parseDiagCurrentScope = this;
    }

    Scope::~Scope() {
        parseDiagCurrentScope = previous;
    }

    bool Scope::hasError() const {
        return Validation::HasError(collected);
    }

    const QList<Issue> &Scope::issues() const {
        return collected;
    }

    QList<Issue> Scope::take() {
        return std::exchange(collected, {});
    }

    bool Active() {
        return parseDiagCurrentScope != nullptr;
    }

    void Report(Issue issue) {
        if (parseDiagCurrentScope == nullptr) return;
        parseDiagCurrentScope->collected.append(std::move(issue));
    }

    void Warn(const QString &field, const QString &message, const QString &value) {
        if (parseDiagCurrentScope == nullptr) return;
        Report({Severity::Warning, field, message, value});
    }

    void Error(const QString &field, const QString &message, const QString &value) {
        if (parseDiagCurrentScope == nullptr) return;
        Report({Severity::Error, field, message, value});
    }

    bool Accept(const QString &field, const QString &message, const QString &value, Severity severity) {
        if (message.isEmpty()) return true;
        if (parseDiagCurrentScope != nullptr) Report({severity, field, message, value});
        return false;
    }
}
