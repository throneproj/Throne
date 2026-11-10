#include "include/global/OtpPlaceholder.hpp"

#include "include/database/DatabaseManager.h"
#include "include/database/OtpProfilesRepo.h"

namespace Configs {
    namespace {
        thread_local int otpSuppressionDepth = 0;
    }

    ScopedOtpSuppression::ScopedOtpSuppression() { otpSuppressionDepth++; }

    ScopedOtpSuppression::~ScopedOtpSuppression() { otpSuppressionDepth--; }

    QString ResolveOtpCode(int otpProfileId)
    {
        if (otpSuppressionDepth > 0 || otpProfileId < 0 || dataManager == nullptr || dataManager->otpProfilesRepo == nullptr) return {};
        auto profile = dataManager->otpProfilesRepo->GetOtpProfile(otpProfileId);
        if (profile == nullptr || !profile->Validate().isEmpty()) return {};
        // HOTP has no time window, so every config build consumes one counter step.
        if (profile->type == OTP::Type::HOTP) {
            profile->counter++;
            dataManager->otpProfilesRepo->Save(profile);
        }
        return profile->CurrentCode();
    }

    QString SubstituteOtp(const QString &text, const QString &code)
    {
        if (code.isEmpty() || text.isEmpty()) return text;
        return QString(text).replace(QString::fromLatin1(kOtpPlaceholder), code);
    }
}
