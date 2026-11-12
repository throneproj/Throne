#pragma once

#include <QString>

namespace Configs
{
    inline constexpr auto kOtpPlaceholder = "{otp}";

    // Advances and persists the counter for HOTP entries.
    QString ResolveOtpCode(int otpProfileId);

    // While alive, ResolveOtpCode() on this thread answers empty and leaves HOTP counters alone.
    class ScopedOtpSuppression {
    public:
        ScopedOtpSuppression();
        ~ScopedOtpSuppression();
        ScopedOtpSuppression(const ScopedOtpSuppression &) = delete;
        ScopedOtpSuppression &operator=(const ScopedOtpSuppression &) = delete;
    };

    QString SubstituteOtp(const QString &text, const QString &code);
}
