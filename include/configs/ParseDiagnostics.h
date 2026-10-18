#pragma once

#include "include/global/Validation.hpp"

// Opt-in import diagnostics: the innermost live Scope on the current thread collects every report.
// Without a Scope (DB loads, clones, scans) every call is a no-op.
namespace Configs::ParseDiag {
    using Validation::Issue;
    using Validation::Severity;

    class Scope {
    public:
        Scope();
        ~Scope();
        Scope(const Scope &) = delete;
        Scope &operator=(const Scope &) = delete;

        [[nodiscard]] bool hasError() const;
        [[nodiscard]] const QList<Issue> &issues() const;
        QList<Issue> take();

    private:
        friend void Report(Issue issue);
        QList<Issue> collected;
        Scope *previous;
    };

    [[nodiscard]] bool Active();

    void Report(Issue issue);
    void Warn(const QString &field, const QString &message, const QString &value = {});
    void Error(const QString &field, const QString &message, const QString &value = {});

    // Reports a non-empty validation message and returns whether the value is acceptable.
    bool Accept(const QString &field, const QString &message, const QString &value = {},
                Severity severity = Severity::Warning);
}
